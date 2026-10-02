#pragma once

#include "Core/Framebuffer.h"
#include "Core/Math/MathTypes.h"
#include "Core/Shader.h"
#include <cstdint>
#include <functional>
#include <vector>

namespace Elysium {

class RenderContext;

// Omnidirectional shadows for point lights, cast by World3D models (RenderCompositor::Render3D).
// One row per light, six
// kTileSize tiles per row, one per cube face (+X -X +Y -Y +Z -Z, each looking along that
// axis with up +Y, or +Z for the two Y faces). Each texel holds the distance from the light
// to the nearest caster in that direction, over the light's radius (1 = nothing within it).
// Render3D's shader picks the face by the major axis and reads it back the same way.
class ShadowAtlas {
   public:
    static constexpr int kMaxLights = 8;
    static constexpr int kTileSize = 256;

    static constexpr unsigned int kNoOwner = 0xFFFFFFFFu;

    // `maxLights`: rows in the atlas (World3D layers shadow up to 16 lights).
    explicit ShadowAtlas(int maxLights = kMaxLights) : maxLights_(maxLights) {}

    struct Light {
        Elysium::Vector3 position;
        float radius = 1.0f;
        unsigned int owner = kNoOwner;  // skips the triangles with the same owner
    };

    // The casters: three world-space vertices per triangle, and one owner per triangle.
    struct Casters {
        std::vector<Elysium::Vector3> triangles;
        std::vector<unsigned int> owners;
    };

    // Lights past Rows() are ignored; each light only draws the triangles that reach into its
    // radius. A row is redrawn only when its light changed or Invalidate touched it since it was
    // last drawn, so still lights over still models cost nothing. `gather` is only called (once)
    // when some row needs redrawing.
    void Render(RenderContext& ctx, const std::vector<Light>& lights, const std::function<void(Casters&)>& gather);
    // A caster changed inside this box (world space): redraw the rows whose light reaches it.
    void Invalidate(Elysium::Vector3 min, Elysium::Vector3 max);

    unsigned int TextureId() const { return atlas_.TextureId(); }
    int LightCount() const { return lightCount_; }
    int Rows() const { return maxLights_; }

   private:
    Framebuffer atlas_;
    Shader shader_;
    bool shaderFailed_ = false;
    int maxLights_ = kMaxLights;
    int lightCount_ = 0;
    // What each row last drew.
    struct Row {
        bool valid = false;
        Light light;
    };
    std::vector<Row> rows_;
};

}  // namespace Elysium
