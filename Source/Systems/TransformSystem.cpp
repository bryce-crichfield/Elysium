#include "Systems/TransformSystem.h"
#include "Core/SystemRegistry.h"
#include "Components/TransformComponent.h"
#include "raymath.h"
#include <vector>

namespace Elysium::Systems {

void TransformSystem::Update(float /*deltaTime*/) {
    std::vector<Entity> roots;
    world->Query<TransformComponent>([&](Entity e, TransformComponent&) {
        Entity parent = world->GetParent(e);
        bool isRoot = (parent == INVALID_ENTITY) || !world->HasComponent<TransformComponent>(parent);
        if (isRoot) roots.push_back(e);
    });

    for (Entity root : roots) {
        ComposeRecursive(root, nullptr, 0);
    }
}

void TransformSystem::ComposeRecursive(Entity entity, const TransformComponent* parentWorld, uint32_t depth) {
    auto& t = world->GetComponent<TransformComponent>(entity);
    t.worldDepth = depth;

    if (!parentWorld) {
        t.worldX = t.localX;
        t.worldY = t.localY;
        t.worldScaleX = t.localScaleX;
        t.worldScaleY = t.localScaleY;
        t.worldRotation = t.localRotation;
    } else {
        float rad = parentWorld->worldRotation * DEG2RAD;
        float scaledX = t.localX * parentWorld->worldScaleX;
        float scaledY = t.localY * parentWorld->worldScaleY;
        float cs = cosf(rad);
        float sn = sinf(rad);

        t.worldX = parentWorld->worldX + (scaledX * cs - scaledY * sn);
        t.worldY = parentWorld->worldY + (scaledX * sn + scaledY * cs);
        t.worldScaleX = parentWorld->worldScaleX * t.localScaleX;
        t.worldScaleY = parentWorld->worldScaleY * t.localScaleY;
        t.worldRotation = parentWorld->worldRotation + t.localRotation;
    }

    // Snapshot children before recursing: GetChildren returns a reference into
    // childrenMap_, and this system only reads it, but copying keeps the
    // traversal safe if that ever changes (mirrors UiSystem's own DFS).
    std::vector<Entity> children(world->GetChildren(entity));
    for (Entity child : children) {
        if (world->HasComponent<TransformComponent>(child)) {
            ComposeRecursive(child, &t, depth + 1);
        }
    }
}

}  // namespace Elysium::Systems

REGISTER_SYSTEM(Elysium::Systems::TransformSystem)
