#include "Core/ShadowAtlas.h"
#include "Core/Common.h"
#include "Core/Graphics.h"
#include "Core/Log.h"
#include "Core/RenderContext.h"

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

extern "C" void* glfwGetProcAddress(const char* name);

#include <algorithm>
#include <cmath>

namespace Elysium {

namespace {

// Vertices are already in world space (the modelview is only the face's view), so the
// fragment's distance to the light is just the interpolated position's.
const char* kVertexSource = R"(#version 330
in vec3 vertexPosition;
uniform mat4 mvp;
out vec3 worldPosition;
void main()
{
    worldPosition = vertexPosition;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

const char* kFragmentSource = R"(#version 330
in vec3 worldPosition;
uniform vec3 e_LightPos;
uniform float e_Radius;
out vec4 finalColor;
void main()
{
    finalColor = vec4(length(worldPosition - e_LightPos) / e_Radius, 0.0, 0.0, 1.0);
}
)";

struct Face {
    ::Vector3 forward;
    ::Vector3 up;
};
// Must match the face table in RenderSystem.cpp's kLight3DSource.
const Face kFaces[6] = {
    {{1, 0, 0}, {0, 1, 0}},  {{-1, 0, 0}, {0, 1, 0}},
    {{0, 1, 0}, {0, 0, 1}},  {{0, -1, 0}, {0, 0, 1}},
    {{0, 0, 1}, {0, 1, 0}},  {{0, 0, -1}, {0, 1, 0}},
};

// Whether a scissor (a camera's viewport) is on: the atlas must clear and draw all of itself.
bool ScissorEnabled() {
    using IsEnabledFn = unsigned char (*)(unsigned int);
    static const auto isEnabled = reinterpret_cast<IsEnabledFn>(glfwGetProcAddress("glIsEnabled"));
    constexpr unsigned int kScissorTest = 0x0C11;  // GL_SCISSOR_TEST
    return isEnabled && isEnabled(kScissorTest);
}

// Whether a box (relative to the light) reaches into cube face `f`'s pyramid: some point in it
// has its coordinate along the face's axis at least as large as the other two in magnitude.
bool BoxInFace(int f, const float lo[3], const float hi[3]) {
    const int axis = f / 2;
    const float along = (f % 2 == 0) ? hi[axis] : -lo[axis];
    if (along <= 0.0f) return false;
    for (int k = 0; k < 3; ++k) {
        if (k == axis) continue;
        const float nearest = (lo[k] <= 0.0f && hi[k] >= 0.0f) ? 0.0f : std::min(std::fabs(lo[k]), std::fabs(hi[k]));
        if (along < nearest) return false;
    }
    return true;
}


// The squared distance from p to the box [lo, hi] (0 inside it).
inline float BoxDistance2(const float lo[3], const float hi[3], const float p[3]) {
    float d2 = 0.0f;
    for (int k = 0; k < 3; ++k) {
        const float d = p[k] < lo[k] ? lo[k] - p[k] : (p[k] > hi[k] ? p[k] - hi[k] : 0.0f);
        d2 += d * d;
    }
    return d2;
}

bool SameLight(const ShadowAtlas::Light& a, const ShadowAtlas::Light& b) {
    return a.position.x == b.position.x && a.position.y == b.position.y && a.position.z == b.position.z &&
           a.radius == b.radius && a.owner == b.owner;
}

// The current scissor box (x, y, width, height).
void ScissorBox(int box[4]) {
    using GetIntegervFn = void (*)(unsigned int, int*);
    static const auto getIntegerv = reinterpret_cast<GetIntegervFn>(glfwGetProcAddress("glGetIntegerv"));
    constexpr unsigned int kScissorBox = 0x0C10;  // GL_SCISSOR_BOX
    if (getIntegerv) getIntegerv(kScissorBox, box);
}

constexpr float kNear = 1.0f;
constexpr float kFar = 4096.0f;

}  // namespace

void ShadowAtlas::Invalidate(Elysium::Vector3 min, Elysium::Vector3 max) {
    for (Row& row : rows_) {
        if (!row.valid) continue;
        const Elysium::Vector3& p = row.light.position;
        const float dx = std::max({min.x - p.x, 0.0f, p.x - max.x});
        const float dy = std::max({min.y - p.y, 0.0f, p.y - max.y});
        const float dz = std::max({min.z - p.z, 0.0f, p.z - max.z});
        const float r = std::max(row.light.radius, 1.0f);
        if (dx * dx + dy * dy + dz * dz < r * r) row.valid = false;
    }
}

void ShadowAtlas::Render(RenderContext& ctx, const std::vector<Light>& lights, uint64_t castersVersion,
                         const std::function<void(Casters&)>& gather) {
    if (!shader_.IsValid() && !shaderFailed_) {
        std::string error;
        shader_ = Shader::FromSource(kVertexSource, kFragmentSource, &error);
        if (!shader_.IsValid()) {
            shaderFailed_ = true;
            Log::Error("Render", "ShadowAtlas shader: " + error);
        }
    }
    lightCount_ = 0;
    if (!shader_.IsValid()) return;
    if (!atlas_.IsValid()) {
        atlas_ = Framebuffer(kTileSize * 6, kTileSize * maxLights_, true);
        rows_.assign(maxLights_, Row{});
    }

    lightCount_ = std::min((int)lights.size(), maxLights_);
    std::vector<int> dirty;
    for (int i = 0; i < lightCount_; ++i) {
        const Row& row = rows_[i];
        if (!row.valid || !SameLight(row.light, lights[i])) dirty.push_back(i);
    }
    ProfileN("ShadowAtlas Render");
    ProfileValue("lights", lightCount_);
    ProfileValue("dirty", dirty.size());
    if (dirty.empty()) return;

    // The casters live on the GPU, uploaded again only when they changed.
    if (!uploaded_ || castersVersion != castersVersion_) {
        ProfileN("ShadowAtlas: Upload");
        Casters casters;
        gather(casters);
        groups_ = std::move(casters.groups);
        if (vbo_) rlUnloadVertexBuffer(vbo_);
        if (vao_) rlUnloadVertexArray(vao_);
        vbo_ = vao_ = 0;
        vertexCount_ = (int)casters.triangles.size();
        if (vertexCount_ > 0) {
            vao_ = rlLoadVertexArray();
            rlEnableVertexArray(vao_);
            vbo_ = rlLoadVertexBuffer(casters.triangles.data(), vertexCount_ * (int)sizeof(Elysium::Vector3), false);
            rlSetVertexAttribute(0, 3, RL_FLOAT, false, 0, 0);  // vertexPosition
            rlEnableVertexAttribute(0);
            rlDisableVertexArray();
        }
        castersVersion_ = castersVersion;
        uploaded_ = true;
        ProfileValue("triangles", vertexCount_ / 3);
    }

    const bool scissor = ScissorEnabled();
    int scissorBox[4] = {0, 0, 0, 0};
    ScissorBox(scissorBox);
    rlDisableScissorTest();
    ctx.BeginRenderTarget(atlas_);
    rlDrawRenderBatchActive();
    rlEnableDepthTest();
    rlDisableBackfaceCulling();

    const ::Matrix projection = MatrixPerspective(90.0 * DEG2RAD, 1.0, kNear, kFar);
    const int mvpLocation = rlGetLocationUniform(shader_.Id(), "mvp");
    for (int i : dirty) {
        ProfileN("ShadowAtlas: Light");
        const Light& light = lights[i];
        rows_[i] = {true, light};
        // Clear just this row (glClear honours the scissor).
        rlEnableScissorTest();
        rlScissor(0, i * kTileSize, kTileSize * 6, kTileSize);
        ctx.ClearTarget(Colors::White);
        rlDisableScissorTest();
        if (!vao_) continue;

        // Whole models: those in reach, and which cube faces their boxes reach into. The GPU
        // clips the rest triangle by triangle.
        for (auto& face : faces_) face.clear();
        {
            ProfileN("ShadowAtlas: Near Models");
            const float p[3] = {light.position.x, light.position.y, light.position.z};
            const float r = std::max(light.radius, 1.0f);
            for (size_t g = 0; g < groups_.size(); ++g) {
                const Group& group = groups_[g];
                if (group.count == 0 || (light.owner != kNoOwner && group.owner == light.owner)) continue;
                const float glo[3] = {group.min.x, group.min.y, group.min.z};
                const float ghi[3] = {group.max.x, group.max.y, group.max.z};
                if (BoxDistance2(glo, ghi, p) >= r * r) continue;
                const float lo[3] = {glo[0] - p[0], glo[1] - p[1], glo[2] - p[2]};
                const float hi[3] = {ghi[0] - p[0], ghi[1] - p[1], ghi[2] - p[2]};
                for (int f = 0; f < 6; ++f) {
                    if (BoxInFace(f, lo, hi)) faces_[f].push_back(g);
                }
            }
        }

        shader_.SetUniform("e_LightPos", Value{light.position});
        shader_.SetUniform("e_Radius", Value{std::max(light.radius, 1.0f)});
        const ::Vector3 eye{light.position.x, light.position.y, light.position.z};
        rlEnableShader(shader_.Id());
        rlEnableVertexArray(vao_);
        for (int f = 0; f < 6; ++f) {
            if (faces_[f].empty()) continue;
            ProfileN("ShadowAtlas: Face");
            rlViewport(f * kTileSize, i * kTileSize, kTileSize, kTileSize);
            const ::Matrix view = MatrixLookAt(eye, Vector3Add(eye, kFaces[f].forward), kFaces[f].up);
            rlSetUniformMatrix(mvpLocation, MatrixMultiply(view, projection));
            // Neighbouring groups are neighbouring in the buffer: one draw per run of them.
            size_t runFirst = 0, runEnd = 0;
            for (size_t g : faces_[f]) {
                const Group& group = groups_[g];
                if (runEnd != runFirst && group.first == runEnd) {
                    runEnd += group.count;
                    continue;
                }
                if (runEnd != runFirst) rlDrawVertexArray((int)runFirst * 3, (int)(runEnd - runFirst) * 3);
                runFirst = group.first;
                runEnd = group.first + group.count;
            }
            if (runEnd != runFirst) rlDrawVertexArray((int)runFirst * 3, (int)(runEnd - runFirst) * 3);
        }
        rlDisableVertexArray();
        rlDisableShader();
    }

    rlEnableBackfaceCulling();
    rlDisableDepthTest();
    ctx.EndRenderTarget();
    // Clearing rows moved the scissor box: put the camera's back.
    rlScissor(scissorBox[0], scissorBox[1], scissorBox[2], scissorBox[3]);
    if (scissor) rlEnableScissorTest();
}

}  // namespace Elysium
