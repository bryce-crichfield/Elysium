#pragma once

#include "Core/Framebuffer.h"
#include "Core/MathTypes.h"
#include "Core/Shader.h"
#include <vector>

namespace Elysium {

class RenderContext;

// Omnidirectional shadows for point lights, in the 3D world lit layers reconstruct
// (RenderCompositor::RenderLit with SceneLayer::pointLights). One row per light, six
// kTileSize tiles per row, one per cube face (+X -X +Y -Y +Z -Z, each looking along that
// axis with up +Y, or +Z for the two Y faces). Each texel holds the distance from the light
// to the nearest caster in that direction, over the light's radius (1 = nothing within it).
// Lighting.fs picks the face by the major axis and reads it back the same way.
class ShadowAtlas {
   public:
    static constexpr int kMaxLights = 8;
    static constexpr int kTileSize = 256;

    static constexpr unsigned int kNoOwner = 0xFFFFFFFFu;

    struct Light {
        Elysium::Vector3 position;
        float radius = 1.0f;
        unsigned int owner = kNoOwner;  // skips the triangles with the same owner
    };

    // `triangles`: the casters, three world-space vertices per triangle; `owners` one entry
    // per triangle. Lights past kMaxLights are ignored.
    void Render(RenderContext& ctx, const std::vector<Light>& lights, const std::vector<Elysium::Vector3>& triangles,
                const std::vector<unsigned int>& owners);

    unsigned int TextureId() const { return atlas_.TextureId(); }
    int LightCount() const { return lightCount_; }

   private:
    Framebuffer atlas_;
    Shader shader_;
    bool shaderFailed_ = false;
    int lightCount_ = 0;
};

}  // namespace Elysium
