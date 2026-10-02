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

std::optional<Contact> NarrowPhase(const ColliderComponent& a, Vector2 posA,
                                   const ColliderComponent& b, Vector2 posB, float isoRatio) {
    const ScaledSpace space{{1.0f, isoRatio}};
    const ColliderShape shapeA = a.ResolvedShape();
    const ColliderShape shapeB = b.ResolvedShape();
    const Vector2 centerA = a.GetCenter(posA.x, posA.y);
    const Vector2 centerB = b.GetCenter(posB.x, posB.y);

    if (shapeA == ColliderShape::Circle && shapeB == ColliderShape::Circle) {
        return CircleVsCircle(centerA, a.radius, centerB, b.radius, space);
    }
    // A Box's GetPolygon is its four AABB corners, so Box and Polygon share this path.
    if (shapeA == ColliderShape::Circle) {
        return CircleVsPolygon(centerA, a.radius, b.GetPolygon(posB.x, posB.y), space);
    }
    if (shapeB == ColliderShape::Circle) {
        return Flip(CircleVsPolygon(centerB, b.radius, a.GetPolygon(posA.x, posA.y), space));
    }
    // Neither is a circle: keep the historical AABB overlap and let PhysicsResponseSystem's
    // axis-aligned code separate it.
    if (!a.GetRect(posA.x, posA.y).Intersects(b.GetRect(posB.x, posB.y))) return std::nullopt;
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
    // Clear previous frame's collisions
    collisions_.clear();
    contacts_.clear();

    // Collect all collidable entities
    struct CollidableEntity {
        Entity entity;
        ColliderComponent collider;
        bool isTrigger;
    };
    std::vector<CollidableEntity> collidables;

    world->Query<TransformComponent, ColliderComponent>(
        [&](Entity e, auto& transform, auto& collider) {
            CollidableEntity ce;
            ce.entity = e;
            ce.collider = collider;
            ce.isTrigger = collider.isTrigger;
            collidables.push_back(ce);
        });

    // O(n^2) broad phase - simple for now
    // TODO: a spatial hash for the broad phase
    // TODO: Consider camera and viewport for math. (if we change zoom, collisions break because colliders are sized for 1:1 pixels)
    for (size_t i = 0; i < collidables.size(); ++i) {
        for (size_t j = i + 1; j < collidables.size(); ++j) {
            const auto& a = collidables[i];
            const auto& b = collidables[j];

            const auto& transformA = world->GetComponent<TransformComponent>(a.entity);
            const auto& transformB = world->GetComponent<TransformComponent>(b.entity);
            const Vector2 posA{transformA.worldX, transformA.worldY};
            const Vector2 posB{transformB.worldX, transformB.worldY};

            // Heights first: a flier over a low wall never touches it.
            float lowA, highA, lowB, highB;
            a.collider.HeightRange(transformA.worldZ, lowA, highA);
            b.collider.HeightRange(transformB.worldZ, lowB, highB);
            if (highA < lowB || highB < lowA) continue;

            // Broad phase: bounding rects, which for a circle cover it conservatively.
            if (!a.collider.GetBroadRect(posA.x, posA.y).Intersects(b.collider.GetBroadRect(posB.x, posB.y))) continue;

            auto contact = NarrowPhase(a.collider, posA, b.collider, posB, isoRatio_);
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
