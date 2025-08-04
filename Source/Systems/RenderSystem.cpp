#include "Systems/RenderSystem.h"
#include "raylib.h"

namespace Elysium::Systems {

void RenderSystem::Render() {
    // Render circles using iterator-based approach
    world->ForEachEntityWith<PositionComponent, CircleRenderComponent>([&](Entity entity) {
        auto& pos = world->GetComponent<PositionComponent>(entity);
        auto& circle = world->GetComponent<CircleRenderComponent>(entity);
        
        DrawCircleV({pos.x, pos.y}, circle.radius, circle.color);
        if (circle.drawOutline) {
            DrawCircleLinesV({pos.x, pos.y}, circle.radius, circle.outlineColor);
        }
    });
    
    // Render text using iterator-based approach
    world->ForEachEntityWith<PositionComponent, TextComponent>([&](Entity entity) {
        auto& pos = world->GetComponent<PositionComponent>(entity);
        auto& text = world->GetComponent<TextComponent>(entity);
        
        DrawText(text.content.c_str(), (int)pos.x, (int)pos.y, text.fontSize, text.color);
    });
}

} // namespace Elysium::Systems