#include "Systems/UiSystem.h"
#include "Core/Components.h"
#include "Core/SystemRegistry.h"
#include "Services/LogService.h"

namespace Elysium::Systems {

void UiSystem::TraverseTree(Entity root, std::function<void(Entity)> visitor) {
    visitor(root);
    for (Entity child : world->GetChildren(root)) {
        if (world->HasComponent<UiComponent>(child)) {
            TraverseTree(child, visitor);
        }
    }
}

void UiSystem::Update(float /*deltaTime*/) {
    // Collect UI roots: have UiComponent and no parent that is also a UI entity
    std::vector<Entity> roots;
    world->Query<UiComponent>([&](Entity e, UiComponent&) {
        Entity parent = world->GetParent(e);
        bool isRoot = (parent == INVALID_ENTITY) ||
                      !world->HasComponent<UiComponent>(parent);
        if (isRoot) roots.push_back(e);
    });

    // DFS from each root — parents are visited before their children, so layout
    // writes are always computed from an already-resolved parent position.
    for (Entity root : roots) {
        TraverseTree(root, [&](Entity e) {
            UiComponent& ui = world->GetComponent<UiComponent>(e);
            if (ui.containerType == UiContainerType::None) return;
            if (!world->HasComponent<PositionComponent>(e)) return;

            PositionComponent& origin = world->GetComponent<PositionComponent>(e);
            float offset = 0.0f;

            for (Entity child : world->GetChildren(e)) {
                if (!world->HasComponent<UiComponent>(child)) continue;
                if (!world->HasComponent<PositionComponent>(child)) continue;

                PositionComponent& childPos = world->GetComponent<PositionComponent>(child);

                float childW = 0.0f, childH = 0.0f;
                if (world->HasComponent<RectangleComponent>(child)) {
                    auto& rect = world->GetComponent<RectangleComponent>(child);
                    childW = rect.width;
                    childH = rect.height;
                }

                // Measure the container itself for alignment offset calculations.
                float containerW = 0.0f, containerH = 0.0f;
                if (world->HasComponent<RectangleComponent>(e)) {
                    auto& rect = world->GetComponent<RectangleComponent>(e);
                    containerW = rect.width;
                    containerH = rect.height;
                }

                auto alignOffset = [](UiAlignment align, float containerSize, float childSize) -> float {
                    switch (align) {
                        case UiAlignment::Middle: return (containerSize - childSize) * 0.5f;
                        case UiAlignment::End:    return  containerSize - childSize;
                        default:                  return 0.0f;  // Start
                    }
                };

                if (ui.containerType == UiContainerType::Vertical) {
                    // Main axis: y (stacked top-down).  Cross axis: x (aligned).
                    childPos.x = origin.x + alignOffset(ui.alignHorizontal, containerW, childW);
                    childPos.y = origin.y + offset;
                    offset += childH + ui.gap;
                } else {  // Horizontal
                    // Main axis: x (stacked left-right).  Cross axis: y (aligned).
                    childPos.x = origin.x + offset;
                    childPos.y = origin.y + alignOffset(ui.alignVertical, containerH, childH);
                    offset += childW + ui.gap;
                }
            }
        });
    }
}

REGISTER_SYSTEM(Elysium::Systems::UiSystem);

}  // namespace Elysium::Systems
