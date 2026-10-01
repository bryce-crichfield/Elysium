#include "Systems/VisibilitySystem.h"
#include "Systems/OcclusionSystem.h"
#include "Core/SystemRegistry.h"
#include "Core/Entity.h"
#include "Core/World.h"
#include "Core/Geometry.h"
#include "Components/TransformComponent.h"
#include "Components/LayerComponent.h"
#include "Components/LightComponent.h"
#include "Components/OccluderComponent.h"
#include <cmath>

namespace Elysium::Systems {

namespace {

// The fog of war's question, asked of the world instead of the picture: can any vision light
// (LightComponent::vision) see this point? Lighting.fs answers it per pixel from the shadow
// maps; this answers it per unit from the occluder footprints, so the two agree: in range
// (3D distance, the ground's y doubled as in WorldTo3D) and no occluder taller than the
// eye standing between them.
struct Eye {
    Entity entity;
    Vector2 ground;
    float height;
    float radius;
};

struct Blocker {
    Entity entity;
    std::vector<Vector2> footprint;
    Rectangle bounds;
    float height;
};

float Cross(Vector2 a, Vector2 b) { return a.x * b.y - a.y * b.x; }

bool SegmentsCross(Vector2 a, Vector2 b, Vector2 c, Vector2 d) {
    const Vector2 r{b.x - a.x, b.y - a.y}, s{d.x - c.x, d.y - c.y};
    const float denom = Cross(r, s);
    if (std::fabs(denom) < 1e-6f) return false;
    const Vector2 ac{c.x - a.x, c.y - a.y};
    const float t = Cross(ac, s) / denom, u = Cross(ac, r) / denom;
    return t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f;
}

bool Blocks(const Blocker& blocker, Vector2 from, Vector2 to) {
    const Rectangle span{std::min(from.x, to.x), std::min(from.y, to.y), std::fabs(to.x - from.x) + 1.0f,
                         std::fabs(to.y - from.y) + 1.0f};
    if (!span.Intersects(blocker.bounds)) return false;
    const auto& f = blocker.footprint;
    for (size_t i = 0; i < f.size(); ++i)
        if (SegmentsCross(from, to, f[i], f[(i + 1) % f.size()])) return true;
    return false;
}

// How much of a vision radius counts as seen: Lighting.fs fades sight out over its last fifth.
constexpr float kSightReach = 0.9f;

}  // namespace

void VisibilitySystem::Update(float) {
    std::vector<Eye> eyes;
    world->Query<TransformComponent, LightComponent>([&](Entity e, auto& transform, auto& light) {
        if (light.vision) eyes.push_back({e, {transform.worldX, transform.worldY}, light.height, light.radius});
    });

    std::vector<Blocker> blockers;
    world->Query<TransformComponent, OccluderComponent>([&](Entity e, auto&, auto& occluder) {
        if (!occluder.castsShadow) return;
        OccluderVolume volume = ResolveOccluder(*world, e);
        if (volume.height <= 0.0f || volume.footprint.size() < 3) return;
        blockers.push_back({e, std::move(volume.footprint), {}, volume.height});
        blockers.back().bounds = PolygonBounds(blockers.back().footprint);
    });

    auto seen = [&](Entity target, Vector2 at) {
        for (const Eye& eye : eyes) {
            const float dx = at.x - eye.ground.x, dz = (at.y - eye.ground.y) * 2.0f;
            if (std::sqrt(dx * dx + dz * dz) > eye.radius * kSightReach) continue;
            bool blocked = false;
            for (const Blocker& b : blockers) {
                if (b.entity == eye.entity || b.entity == target || b.height < eye.height) continue;
                if (Blocks(b, eye.ground, at)) { blocked = true; break; }
            }
            if (!blocked) return true;
        }
        return false;
    };

    // What hides in fog (LayerComponent::hideInFog) is hidden while no vision light sees it;
    // RenderSorter hides its children with it. With no vision lights there's no fog.
    world->Query<TransformComponent, LayerComponent>([&](Entity e, auto& transform, auto& layer) {
        layer.inFog = layer.hideInFog && !eyes.empty() && !seen(e, {transform.worldX, transform.worldY});
    });
}

}  // namespace Elysium::Systems

REGISTER_SYSTEM(Elysium::Systems::VisibilitySystem)
