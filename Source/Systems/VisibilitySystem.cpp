#include "Systems/VisibilitySystem.h"
#include "Core/SystemRegistry.h"
#include "Core/Entity.h"
#include "Components/PositionComponent.h"
#include "Components/LayerComponent.h"
#include "Components/TeamComponent.h"
#include "Components/LightComponent.h"
#include "Components/ColliderComponent.h"
#include "Components/ParentComponent.h"
#include "raymath.h"

namespace Elysium::Systems {

void VisibilitySystem::Update(float deltaTime) {
    // Collect friendly vision sources.
    // A vision source is a light entity (LightComponent + ColliderComponent + ParentComponent)
    // whose parent is a player unit (TeamComponent.team == 0).
    // The ColliderComponent.width defines the vision diameter; we use width * 0.5 as the radius.
    // This keeps the visual light radius (LightComponent.radius) independent from game logic.
    // Isometric ellipse: radiusX = collider.width * 0.5, radiusY = collider.height * 0.5.
    // A point (px, py) is inside if (dx/radiusX)^2 + (dy/radiusY)^2 <= 1.
    // Setting width:height = tileWidth:tileHeight (2:1) makes the ellipse appear
    // as a circle when viewed through the isometric projection.
    struct VisionSource {
        Vector2 position;
        float   radiusX;
        float   radiusY;
    };
    std::vector<VisionSource> sources;

    world->Query<PositionComponent, LightComponent, ColliderComponent, ParentComponent>(
        [&](Entity e, auto& pos, auto& light, auto& collider, auto& parentComp) {
            if (parentComp.parent == INVALID_ENTITY) return;
            if (!world->HasComponent<TeamComponent>(parentComp.parent)) return;
            if (world->GetComponent<TeamComponent>(parentComp.parent).team != 0) return;
            sources.push_back({{pos.x, pos.y}, collider.width * 0.5f, collider.height * 0.5f});
        });

    // For every enemy unit, check if it falls within any vision ellipse.
    world->Query<PositionComponent, LayerComponent, TeamComponent>(
        [&](Entity e, auto& pos, auto& layer, auto& team) {
            if (team.team == 0) return;

            bool visible = false;
            for (const auto& src : sources) {
                float dx = pos.x - src.position.x;
                float dy = pos.y - src.position.y;
                float nx = dx / src.radiusX;
                float ny = dy / src.radiusY;
                if (nx * nx + ny * ny <= 1.0f) {
                    visible = true;
                    break;
                }
            }
            layer.isVisible = visible;
        });
}

}  // namespace Elysium::Systems

REGISTER_SYSTEM(Elysium::Systems::VisibilitySystem)
