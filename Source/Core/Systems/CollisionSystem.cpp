#include "Core/Systems/CollisionSystem.h"
#include "Core/SystemRegistry.h"
#include "Core/Entity.h"
#include "Core/Math/Polygon.h"
#include "Core/Scene.h"
#include "Core/Components/TransformComponent.h"
#include "Core/Components/ColliderComponent.h"
#include <algorithm>
#include <cmath>
#include <optional>

namespace Elysium::Systems {

namespace {

// Separating two circles, or a circle and a polygon, in the space where a ground circle is a
// circle: y is stretched by isoRatio so a radius measured along x reaches radius/isoRatio along
// y. The separation vector is computed there and mapped back, which keeps it exact — moving by
// the returned normal * depth restores the scaled distance the shapes need.
struct ScaledSpace {
    Vector2 scale;
    Vector2 In(Vector2 p) const { return {p.x * scale.x, p.y * scale.y}; }
    Vector2 Out(Vector2 p) const { return {p.x / scale.x, p.y / scale.y}; }

    // A displacement in scaled space -> a world-space normal and distance.
    std::optional<Contact> Resolve(Vector2 directionScaled, float depthScaled) const {
        float len = directionScaled.Length();
        if (len <= 1e-6f || depthScaled <= 0.0f) return std::nullopt;
        Vector2 world = Out(directionScaled * (depthScaled / len));
        float depth = world.Length();
        if (depth <= 1e-6f) return std::nullopt;
        return Contact{world / depth, depth, true};
    }
};

// Both normals point "push a away from b".
std::optional<Contact> CircleVsCircle(Vector2 centerA, float radiusA, Vector2 centerB, float radiusB,
                                      const ScaledSpace& space) {
    Vector2 delta = space.In(centerA) - space.In(centerB);
    float distance = delta.Length();
    float reach = radiusA + radiusB;
    if (distance >= reach) return std::nullopt;
    // Exactly concentric: no meaningful direction, so nudge along -y (visually "up" and out).
    if (distance <= 1e-6f) return space.Resolve({0.0f, -1.0f}, reach);
    return space.Resolve(delta, reach - distance);
}

// `polygon` is world space. Returns the push for the *circle*.
std::optional<Contact> CircleVsPolygon(Vector2 center, float radius, const Polygon& polygon,
                                       const ScaledSpace& space) {
    const auto nearest = polygon.NearestOnOutline(center, space.scale);
    if (!nearest) return std::nullopt;
    const Polygon::Nearest& near = *nearest;

    Vector2 outward = space.In(center) - space.In(near.point);
    if (near.inside) {
        // Centre is inside: the shortest way out is toward the nearest edge and beyond it.
        return space.Resolve(outward * -1.0f, radius + near.distance);
    }
    if (near.distance >= radius) return std::nullopt;
    return space.Resolve(outward, radius - near.distance);
}

std::optional<Contact> Flip(std::optional<Contact> contact) {
    if (contact) contact->normal = contact->normal * -1.0f;
    return contact;
}

// A collider resolved for this frame: its shape decided and its outline parsed and placed once,
// not once per pair it's tested against.
struct Prepared {
    Entity entity;
    ColliderShape shape;
    Vector2 center;
    float radius;
    Polygon polygon;  // world space; empty for a circle
    Rectangle rect;   // GetRect, for the box-vs-box fallback
    Rectangle broad;  // GetBroadRect
    float low, high;  // height range
};

std::optional<Contact> NarrowPhase(const Prepared& a, const Prepared& b, float isoRatio) {
    const ScaledSpace space{{1.0f, isoRatio}};
    if (a.shape == ColliderShape::Circle && b.shape == ColliderShape::Circle) {
        return CircleVsCircle(a.center, a.radius, b.center, b.radius, space);
    }
    // A Box's polygon is its four AABB corners, so Box and Polygon share this path.
    if (a.shape == ColliderShape::Circle) return CircleVsPolygon(a.center, a.radius, b.polygon, space);
    if (b.shape == ColliderShape::Circle) return Flip(CircleVsPolygon(b.center, b.radius, a.polygon, space));
    // Neither is a circle: keep the historical AABB overlap and let PhysicsResponseSystem's
    // axis-aligned code separate it.
    if (!a.rect.Intersects(b.rect)) return std::nullopt;
    return Contact{};
}

}  // namespace

CollisionSystem::CollisionSystem(Context context) : System(context) {
}

SystemParameters CollisionSystem::DefaultParameters() const {
    return {{"isoRatio", Value{2.0f}}};
}

void CollisionSystem::OnParametersChanged() {
    isoRatio_ = std::max(0.01f, GetParameter("isoRatio", 2.0f));
}

const Contact* CollisionSystem::GetContact(Entity a, Entity b) const {
    auto it = contacts_.find(CollisionPair(a, b));
    return it != contacts_.end() ? &it->second : nullptr;
}

void CollisionSystem::Update(float deltaTime) {
    collisions_.clear();
    contacts_.clear();

    std::vector<Prepared> colliders;
    world->Query<TransformComponent, ColliderComponent>([&](Entity e, auto& transform, auto& collider) {
        const float x = transform.worldX, y = transform.worldY;
        Prepared p{e, collider.ResolvedShape(), collider.GetCenter(x, y), collider.radius, {},
                   collider.GetRect(x, y), collider.GetBroadRect(x, y), 0.0f, 0.0f};
        if (p.shape != ColliderShape::Circle) p.polygon = collider.GetPolygon(x, y);
        collider.HeightRange(transform.worldZ, p.low, p.high);
        colliders.push_back(std::move(p));
    });

    // Broad phase: sweep along x. Sorted by left edge, each collider only meets the ones that
    // start before it ends.
    // TODO: Consider camera and viewport for math. (if we change zoom, collisions break because colliders are sized for 1:1 pixels)
    std::sort(colliders.begin(), colliders.end(), [](const Prepared& a, const Prepared& b) { return a.broad.x < b.broad.x; });
    for (size_t i = 0; i < colliders.size(); ++i) {
        const Prepared& a = colliders[i];
        const float right = a.broad.x + a.broad.width;
        for (size_t j = i + 1; j < colliders.size() && colliders[j].broad.x <= right; ++j) {
            const Prepared& b = colliders[j];
            // Heights first: a flier over a low wall never touches it.
            if (a.high < b.low || b.high < a.low) continue;
            if (!a.broad.Intersects(b.broad)) continue;

            auto contact = NarrowPhase(a, b, isoRatio_);
            if (!contact) continue;

            CollisionPair pair(a.entity, b.entity);
            // NarrowPhase orients the normal against `a`; CollisionPair may have swapped them.
            if (contact->radial && pair.a != a.entity) contact->normal = contact->normal * -1.0f;
            collisions_.insert(pair);
            contacts_.emplace(pair, *contact);
        }
    }
}

bool CollisionSystem::AreColliding(Entity a, Entity b) const {
    return collisions_.find(CollisionPair(a, b)) != collisions_.end();
}

std::vector<Entity> CollisionSystem::GetCollisionsWith(Entity entity) const {
    std::vector<Entity> result;
    for (const auto& pair : collisions_) {
        if (pair.a == entity) {
            result.push_back(pair.b);
        } else if (pair.b == entity) {
            result.push_back(pair.a);
        }
    }
    return result;
}

}  // namespace Elysium::Systems

REGISTER_SYSTEM(Elysium::Systems::CollisionSystem)
