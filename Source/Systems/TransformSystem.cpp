#include "Systems/TransformSystem.h"
#include "Core/SystemRegistry.h"
#include "Components/TransformComponent.h"
#include "Components/ParentComponent.h"
#include "raymath.h"
#include <vector>
#include <unordered_set>

namespace Elysium::Systems {

void TransformSystem::Update(float /*deltaTime*/) {
    // True roots (no parent at all) vs. entities whose real parent just lacks a
    // TransformComponent are different cases: true roots have no adjacency entry
    // to read sibling order from (childrenMap_ only tracks parent->child links,
    // and root-level reordering never populates a childrenMap_[INVALID_ENTITY]
    // entry — see WorldEditor's root-level drag&drop), so their order has to come
    // from Query's entity iteration order instead. Entities under a non-transform
    // parent DO have a real adjacency entry, so they must only be handled via that
    // parent's group below — including them in `roots` as well would double-compose
    // them (once here, once in their parent's group) with two different indices.
    std::vector<Entity> roots;
    std::unordered_set<Entity> nonTransformParents;

    world->Query<TransformComponent>([&](Entity e, TransformComponent&) {
        Entity parent = world->GetParent(e);
        if (parent == INVALID_ENTITY) {
            roots.push_back(e);
        } else if (!world->HasComponent<TransformComponent>(parent)) {
            nonTransformParents.insert(parent);
        }
    });

    uint32_t rootIndex = 0;
    for (Entity root : roots) {
        ComposeRecursive(root, nullptr, 0, rootIndex);
        rootIndex++;
    }

    for (Entity parent : nonTransformParents) {
        // Use the real children list to get true sibling index, not filtered group order.
        const std::vector<Entity>& siblings = world->GetChildren(parent);

        uint32_t childIndex = 0;
        for (Entity sibling : siblings) {
            if (world->HasComponent<TransformComponent>(sibling)) {
                ComposeRecursive(sibling, nullptr, 0, childIndex);
            }
            childIndex++;
        }
    }
}

void TransformSystem::ComposeRecursive(Entity entity, const TransformComponent* parentWorld, uint32_t depth, uint32_t index) {
    auto& t = world->GetComponent<TransformComponent>(entity);
    t.worldDepth = depth;

    if (world->HasComponent<ParentComponent>(entity)) {
        auto& pc = world->GetComponent<ParentComponent>(entity);
        pc.childIndex = index;
    }

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
    uint32_t childIndex = 0;
    for (Entity child : children) {
        if (world->HasComponent<TransformComponent>(child)) {
            ComposeRecursive(child, &t, depth + 1, childIndex);
        }

        childIndex++;
    }
}

}  // namespace Elysium::Systems

REGISTER_SYSTEM(Elysium::Systems::TransformSystem)
