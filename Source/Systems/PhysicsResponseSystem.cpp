#include "Systems/PhysicsResponseSystem.h"
#include "Systems/CollisionSystem.h"
#include "Core/SystemRegistry.h"
#include "Core/Scene.h"
#include "Components/TransformComponent.h"
#include "Components/ColliderComponent.h"
#include "Components/KinematicsComponent.h"
#include <cmath>
#include <algorithm>

namespace Elysium::Systems {

void PhysicsResponseSystem::Update(float deltaTime) {
    auto* collisionSystem = scene->GetSystem<CollisionSystem>();
    if (!collisionSystem) return;

    const auto& collisions = collisionSystem->GetCollisions();

    for (const auto& pair : collisions) {
        Entity a = pair.a;
        Entity b = pair.b;

        if (!world->HasComponent<ColliderComponent>(a) ||
            !world->HasComponent<ColliderComponent>(b)) continue;

        auto& ca = world->GetComponent<ColliderComponent>(a);
        auto& cb = world->GetComponent<ColliderComponent>(b);

        // Let scripts handle trigger overlaps
        if (ca.isTrigger || cb.isTrigger) continue;

        if (!world->HasComponent<TransformComponent>(a) ||
            !world->HasComponent<TransformComponent>(b)) continue;

        bool aKin = world->HasComponent<KinematicsComponent>(a);
        bool bKin = world->HasComponent<KinematicsComponent>(b);
        if (!aKin && !bKin) continue;  // Both static — nothing to push

        auto& transformA = world->GetComponent<TransformComponent>(a);
        auto& transformB = world->GetComponent<TransformComponent>(b);

        // Compute centers (position is entity anchor; collider offset shifts the AABB center)
        float centerAX = transformA.worldX + ca.offsetX;
        float centerAY = transformA.worldY + ca.offsetY;
        float centerBX = transformB.worldX + cb.offsetX;
        float centerBY = transformB.worldY + cb.offsetY;

        float dx = centerBX - centerAX;
        float dy = centerBY - centerAY;

        float overlapX = (ca.width + cb.width)   * 0.5f - std::abs(dx);
        float overlapY = (ca.height + cb.height) * 0.5f - std::abs(dy);

        // CollisionSystem uses CheckCollisionRecs which can produce near-zero overlap on edges;
        // a small guard avoids jitter on resting contacts.
        if (overlapX <= 0.0f || overlapY <= 0.0f) continue;

        // Split the push: if both dynamic, each takes half; if one static, dynamic takes all.
        float aShare = aKin ? (bKin ? 0.5f : 1.0f) : 0.0f;
        float bShare = bKin ? (aKin ? 0.5f : 1.0f) : 0.0f;

        if (overlapX < overlapY) {
            // Minimum penetration is along X — push horizontally
            float signX = (dx >= 0.0f) ? 1.0f : -1.0f;

            if (aKin) {
                // world* is a cache TransformSystem overwrites next frame, so the
                // correction is applied to local* (the delta is the same either way).
                transformA.localX -= signX * overlapX * aShare;
                auto& kinA = world->GetComponent<KinematicsComponent>(a);
                // Cancel velocity component that was pushing into B
                if (signX > 0.0f && kinA.velocity.x > 0.0f) kinA.velocity.x = 0.0f;
                if (signX < 0.0f && kinA.velocity.x < 0.0f) kinA.velocity.x = 0.0f;
            }
            if (bKin) {
                transformB.localX += signX * overlapX * bShare;
                auto& kinB = world->GetComponent<KinematicsComponent>(b);
                if (signX > 0.0f && kinB.velocity.x < 0.0f) kinB.velocity.x = 0.0f;
                if (signX < 0.0f && kinB.velocity.x > 0.0f) kinB.velocity.x = 0.0f;
            }
        } else {
            // Minimum penetration is along Y — push vertically
            float signY = (dy >= 0.0f) ? 1.0f : -1.0f;

            if (aKin) {
                transformA.localY -= signY * overlapY * aShare;
                auto& kinA = world->GetComponent<KinematicsComponent>(a);
                if (signY > 0.0f && kinA.velocity.y > 0.0f) kinA.velocity.y = 0.0f;
                if (signY < 0.0f && kinA.velocity.y < 0.0f) kinA.velocity.y = 0.0f;
            }
            if (bKin) {
                transformB.localY += signY * overlapY * bShare;
                auto& kinB = world->GetComponent<KinematicsComponent>(b);
                if (signY > 0.0f && kinB.velocity.y < 0.0f) kinB.velocity.y = 0.0f;
                if (signY < 0.0f && kinB.velocity.y > 0.0f) kinB.velocity.y = 0.0f;
            }
        }
    }
}

}  // namespace Elysium::Systems

REGISTER_SYSTEM(Elysium::Systems::PhysicsResponseSystem)
