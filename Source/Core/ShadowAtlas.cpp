#include "Core/ShadowAtlas.h"
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

void ShadowAtlas::Render(RenderContext& ctx, const std::vector<Light>& lights,
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
    if (dirty.empty()) return;
    Casters casters;
    gather(casters);
    const std::vector<Elysium::Vector3>& triangles = casters.triangles;
    const std::vector<unsigned int>& owners = casters.owners;

    const bool scissor = ScissorEnabled();
    int scissorBox[4] = {0, 0, 0, 0};
    ScissorBox(scissorBox);
    rlDisableScissorTest();
    ctx.BeginRenderTarget(atlas_);
    rlDrawRenderBatchActive();
    rlEnableDepthTest();
    rlDisableBackfaceCulling();
    ctx.PushShader(shader_);

    const ::Matrix projection = MatrixPerspective(90.0 * DEG2RAD, 1.0, kNear, kFar);
    std::vector<size_t> near, inFace;
    for (int i : dirty) {
        const Light& light = lights[i];
        rows_[i] = {true, light};
        // Clear just this row (glClear honours the scissor).
        rlEnableScissorTest();
        rlScissor(0, i * kTileSize, kTileSize * 6, kTileSize);
        ctx.ClearTarget(Colors::White);
        rlDisableScissorTest();

        // The triangles whose bounds reach into the light's radius.
        near.clear();
        const float r = std::max(light.radius, 1.0f);
        for (size_t t = 0; t < owners.size(); ++t) {
            if (light.owner != kNoOwner && owners[t] == light.owner) continue;
            const Elysium::Vector3& a = triangles[t * 3];
            const Elysium::Vector3& b = triangles[t * 3 + 1];
            const Elysium::Vector3& c = triangles[t * 3 + 2];
            const float dx = std::max({std::min({a.x, b.x, c.x}) - light.position.x, 0.0f, light.position.x - std::max({a.x, b.x, c.x})});
            const float dy = std::max({std::min({a.y, b.y, c.y}) - light.position.y, 0.0f, light.position.y - std::max({a.y, b.y, c.y})});
            const float dz = std::max({std::min({a.z, b.z, c.z}) - light.position.z, 0.0f, light.position.z - std::max({a.z, b.z, c.z})});
            if (dx * dx + dy * dy + dz * dz < r * r) near.push_back(t);
        }
        const ::Vector3 eye{light.position.x, light.position.y, light.position.z};
        shader_.SetUniform("e_LightPos", Value{light.position});
        shader_.SetUniform("e_Radius", Value{std::max(light.radius, 1.0f)});

        for (int f = 0; f < 6; ++f) {
            // Only the triangles inside this face's frustum.
            inFace.clear();
            for (size_t t : near) {
                float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
                for (size_t k = t * 3; k < t * 3 + 3; ++k) {
                    const float v[3] = {triangles[k].x - light.position.x, triangles[k].y - light.position.y,
                                        triangles[k].z - light.position.z};
                    for (int c = 0; c < 3; ++c) { lo[c] = std::min(lo[c], v[c]); hi[c] = std::max(hi[c], v[c]); }
                }
                if (BoxInFace(f, lo, hi)) inFace.push_back(t);
            }
            if (inFace.empty()) continue;
            rlViewport(f * kTileSize, i * kTileSize, kTileSize, kTileSize);
            rlSetMatrixProjection(projection);
            rlSetMatrixModelview(MatrixLookAt(eye, Vector3Add(eye, kFaces[f].forward), kFaces[f].up));
            rlBegin(RL_TRIANGLES);
            rlColor4ub(255, 255, 255, 255);
            for (size_t t : inFace) {
                for (size_t k = t * 3; k < t * 3 + 3; ++k) rlVertex3f(triangles[k].x, triangles[k].y, triangles[k].z);
            }
            rlEnd();
            rlDrawRenderBatchActive();
        }
    }

    ctx.PopShader();
    rlEnableBackfaceCulling();
    rlDisableDepthTest();
    ctx.EndRenderTarget();
    // Clearing rows moved the scissor box: put the camera's back.
    rlScissor(scissorBox[0], scissorBox[1], scissorBox[2], scissorBox[3]);
    if (scissor) rlEnableScissorTest();
}

}  // namespace Elysium
