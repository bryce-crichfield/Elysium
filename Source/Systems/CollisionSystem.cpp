#include "Systems/CollisionSystem.h"
#include "Core/SystemRegistry.h"
#include "Core/Entity.h"
#include "Core/Scene.h"
#include "Components/PositionComponent.h"
#include "Components/ColliderComponent.h"
#include "raylib.h"

namespace Elysium::Systems {

CollisionSystem::CollisionSystem(Context context) : System(context) {
}

void CollisionSystem::Update(float deltaTime) {
    // Clear previous frame's collisions
    collisions_.clear();

    // Collect all collidable entities
    struct CollidableEntity {
        Entity entity;
        Rectangle rect;
        bool isTrigger;
    };
    std::vector<CollidableEntity> collidables;

    world->Query<PositionComponent, ColliderComponent>(
        [&](Entity e, auto& pos, auto& collider) {
            CollidableEntity ce;
            ce.entity = e;
            ce.rect = collider.GetRect(pos.x, pos.y);
            ce.isTrigger = collider.isTrigger;
            collidables.push_back(ce);
        });

    // O(n^2) broad phase - simple for now
    // TODO: Use SpatialSystem for broad phase optimization
    for (size_t i = 0; i < collidables.size(); ++i) {
        for (size_t j = i + 1; j < collidables.size(); ++j) {
            const auto& a = collidables[i];
            const auto& b = collidables[j];

            // AABB intersection test
            if (CheckCollisionRecs(a.rect, b.rect)) {
                collisions_.insert(CollisionPair(a.entity, b.entity));
            }
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
