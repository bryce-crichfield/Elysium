#include "Systems/OcclusionSystem.h"
#include "Core/SystemRegistry.h"
#include "Core/Geometry.h"
#include "Core/World.h"
#include "Components/TransformComponent.h"
#include "Components/OccluderComponent.h"
#include "Components/ColliderComponent.h"
#include "Components/MaterialComponent.h"
#include "Components/RectangleComponent.h"
#include <cmath>

namespace Elysium::Systems {

namespace {

constexpr float kDefaultFootprintW = 32.0f;
constexpr float kDefaultFootprintH = 16.0f;
constexpr int kColumnSamples = 5;
const char* const kTintUniform = "uTint";

template <typename Fn>
void ForSubtree(World& world, Entity root, Fn&& fn) {
    std::vector<Entity> stack{root};
    while (!stack.empty()) {
        Entity e = stack.back();
        stack.pop_back();
        fn(e);
        for (Entity child : world.GetChildren(e)) stack.push_back(child);
    }
}

template <typename Fn>
void ForTextureLayers(World& world, Entity root, Fn&& fn) {
    ForSubtree(world, root, [&](Entity e) {
        if (!world.HasComponent<MaterialComponent>(e)) return;
        auto& layers = world.GetComponent<MaterialComponent>(e).layers;
        for (size_t i = 0; i < layers.size(); ++i) {
            if (layers[i].material == "Texture") fn(e, i, layers[i]);
        }
    });
}

float SpriteHeight(World& world, Entity root) {
    float best = 0.0f;
    ForSubtree(world, root, [&](Entity e) {
        if (!world.HasComponent<RectangleComponent>(e) || !world.HasComponent<MaterialComponent>(e)) return;
        float scale = world.HasComponent<TransformComponent>(e) ? world.GetComponent<TransformComponent>(e).worldScaleY : 1.0f;
        best = std::max(best, std::fabs(world.GetComponent<RectangleComponent>(e).height * scale));
    });
    return best;
}

std::vector<Vector2> LocalFootprint(World& world, Entity e, const OccluderComponent& occluder) {
    if (auto authored = occluder.LocalFootprint(); !authored.empty()) return authored;
    if (world.HasComponent<ColliderComponent>(e)) return world.GetComponent<ColliderComponent>(e).GetPolygon(0.0f, 0.0f);
    return IsoDiamond(kDefaultFootprintW, kDefaultFootprintH);
}

}  // namespace

OccluderVolume ResolveOccluder(World& world, Entity e) {
    const auto& transform = world.GetComponent<TransformComponent>(e);
    const auto& occluder = world.GetComponent<OccluderComponent>(e);

    OccluderVolume v;
    v.entity = e;
    v.anchor = {transform.worldX, transform.worldY};
    v.isStatic = occluder.isStatic;
    v.height = occluder.height;
    if (v.height <= 0.0f && !v.isStatic) v.height = SpriteHeight(world, e);

    v.footprint = TranslatePolygon(LocalFootprint(world, e, occluder), v.anchor);
    std::vector<Vector2> hull = v.footprint;
    if (v.height > 0.0f) {
        for (const auto& p : v.footprint) hull.push_back({p.x, p.y - v.height});
    }
    v.volume = ConvexHull(hull);
    v.volumeBounds = PolygonBounds(v.volume);
    return v;
}

bool IsBehind(const OccluderVolume& a, const OccluderVolume& b) {
    if (a.entity == b.entity || a.anchor.y >= b.anchor.y || b.volume.size() < 3) return false;

    Rectangle column{a.anchor.x - 1.0f, a.anchor.y - a.height, 2.0f, a.height + 1.0f};
    if (!column.Intersects(b.volumeBounds)) return false;

    for (int i = 0; i <= kColumnSamples; ++i) {
        Vector2 p{a.anchor.x, a.anchor.y - a.height * (float)i / kColumnSamples};
        if (PointInPolygon(p, b.volume)) return true;
    }
    return false;
}

void OcclusionSystem::ApplyTint(Entity root, Vector4 tint) {
    TintState& state = tints_[root];
    const bool firstTouch = !state.touched && state.saved.empty();
    state.touched = true;

    ForTextureLayers(*world, root, [&](Entity e, size_t index, MaterialLayer& layer) {
        if (firstTouch) {
            auto it = layer.overrides.find(kTintUniform);
            state.saved.push_back({e, index, it == layer.overrides.end() ? std::nullopt : std::optional<Value>(it->second)});
        }
        layer.overrides[kTintUniform] = Value{tint};
    });
}

void OcclusionSystem::RestoreUntouched() {
    for (auto it = tints_.begin(); it != tints_.end();) {
        TintState& state = it->second;
        if (state.touched) { state.touched = false; ++it; continue; }

        for (const auto& [entity, index, previous] : state.saved) {
            if (!world->IsAlive(entity) || !world->HasComponent<MaterialComponent>(entity)) continue;
            auto& layers = world->GetComponent<MaterialComponent>(entity).layers;
            if (index >= layers.size()) continue;
            if (previous) layers[index].overrides[kTintUniform] = *previous;
            else          layers[index].overrides.erase(kTintUniform);
        }
        it = tints_.erase(it);
    }
}

void OcclusionSystem::Update(float) {
    std::vector<OccluderVolume> volumes;
    world->Query<TransformComponent, OccluderComponent>([&](Entity e, auto&, auto&) {
        volumes.push_back(ResolveOccluder(*world, e));
    });

    for (const auto& a : volumes) {
        if (a.isStatic) continue;
        for (const auto& b : volumes) {
            if (!IsBehind(a, b)) continue;
            const auto& occluder = world->GetComponent<OccluderComponent>(b.entity);
            switch (occluder.Mode()) {
                case OcclusionMode::FadeSelf:     ApplyTint(b.entity, {1.0f, 1.0f, 1.0f, occluder.fadeAlpha}); break;
                case OcclusionMode::TintOccluded: ApplyTint(a.entity, Value{occluder.tint}.As<Vector4>());   break;
                case OcclusionMode::None:         break;
            }
        }
    }
    RestoreUntouched();
}

}  // namespace Elysium::Systems

REGISTER_SYSTEM(Elysium::Systems::OcclusionSystem)
