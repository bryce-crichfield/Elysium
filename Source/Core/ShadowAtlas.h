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

    // The casters: three world-space vertices per triangle, grouped by owner (a model). Each
    // group's box bounds its triangles, so a light skips whole models out of its reach.
    struct Group {
        size_t first = 0;   // its first triangle
        size_t count = 0;   // and how many
        unsigned int owner = kNoOwner;
        Elysium::Vector3 min, max;
    };
    struct Casters {
        std::vector<Elysium::Vector3> triangles;
        std::vector<Group> groups;
    };

    // Lights past Rows() are ignored; each light only draws the models that reach into its
    // radius. A row is redrawn only when its light changed or Invalidate touched it since it was
    // last drawn, so still lights over still models cost nothing. The casters are kept on the
    // GPU: `gather` is only called when some row needs redrawing and `castersVersion` differs
    // from the last upload's (bump it whenever any caster's triangles change).
    void Render(RenderContext& ctx, const std::vector<Light>& lights, uint64_t castersVersion,
                const std::function<void(Casters&)>& gather);
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
    // The casters as uploaded: their groups, and the vertex buffer holding their triangles.
    std::vector<Group> groups_;
    unsigned int vao_ = 0, vbo_ = 0;
    int vertexCount_ = 0;
    uint64_t castersVersion_ = 0;
    bool uploaded_ = false;
    // Scratch, kept between frames: the groups each cube face of the light being drawn sees.
    std::vector<size_t> faces_[6];
};

}  // namespace Elysium
