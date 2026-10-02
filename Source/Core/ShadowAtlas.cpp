#include "Core/ShadowAtlas.h"
#include "Core/Graphics.h"
#include "Core/Log.h"
#include "Core/RenderContext.h"

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

extern "C" void* glfwGetProcAddress(const char* name);

#include <algorithm>

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

constexpr float kNear = 1.0f;
constexpr float kFar = 4096.0f;

}  // namespace

void ShadowAtlas::Render(RenderContext& ctx, const std::vector<Light>& lights,
                         const std::vector<Elysium::Vector3>& triangles, const std::vector<unsigned int>& owners) {
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
    if (!atlas_.IsValid()) atlas_ = Framebuffer(kTileSize * 6, kTileSize * maxLights_, true);

    lightCount_ = std::min((int)lights.size(), maxLights_);

    const bool scissor = ScissorEnabled();
    rlDisableScissorTest();
    ctx.BeginRenderTarget(atlas_);
    ctx.ClearTarget(Colors::White);
    rlDrawRenderBatchActive();
    rlEnableDepthTest();
    rlDisableBackfaceCulling();
    ctx.PushShader(shader_);

    const ::Matrix projection = MatrixPerspective(90.0 * DEG2RAD, 1.0, kNear, kFar);
    std::vector<size_t> near;
    for (int i = 0; i < lightCount_; ++i) {
        const Light& light = lights[i];
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
            rlViewport(f * kTileSize, i * kTileSize, kTileSize, kTileSize);
            rlSetMatrixProjection(projection);
            rlSetMatrixModelview(MatrixLookAt(eye, Vector3Add(eye, kFaces[f].forward), kFaces[f].up));
            rlBegin(RL_TRIANGLES);
            rlColor4ub(255, 255, 255, 255);
            for (size_t t : near) {
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
    if (scissor) rlEnableScissorTest();
}

}  // namespace Elysium
