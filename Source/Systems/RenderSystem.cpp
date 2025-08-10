#include "Systems/RenderSystem.h"
#include "raylib.h"
#include "Application.h"
#include "Scene.h"
#include "Entity.h"
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <variant>

namespace Elysium::Systems {

struct RectangleData {
    float width, height;
    Color background, border;
};

struct CircleData {
    float radius;
    Color background, border;
};

struct TextData {
    std::string content;
    int fontSize;
    Color color;
};

struct RenderItem {
    Entity entity;
    int zIndex;
    enum class Type { Rectangle, Circle, Text } type;
    Vector2 position;
    std::variant<RectangleData, CircleData, TextData> data;
};

void RenderSystem::Render() {
    std::vector<RenderItem> renderItems; // Changed from unordered_map to vector

    // Collect position components and create render items for each render component
    world->ForEachEntityWith<PositionComponent>([&](Entity entity) {
        auto& pos = world->GetComponent<PositionComponent>(entity);
        int zIndex = world->HasComponent<LayerComponent>(entity)
            ? world->GetComponent<LayerComponent>(entity).z
            : 0;

        // Check for RectangleComponent
        if (world->HasComponent<RectangleComponent>(entity)) {
            RenderItem item;
            item.entity = entity;
            item.position = {pos.x, pos.y};
            item.zIndex = zIndex;
            item.type = RenderItem::Type::Rectangle;
            auto& rect = world->GetComponent<RectangleComponent>(entity);
            item.data = RectangleData{rect.width, rect.height, rect.background, rect.border};
            renderItems.push_back(item);
        }

        // Check for CircleComponent
        if (world->HasComponent<CircleComponent>(entity)) {
            RenderItem item;
            item.entity = entity;
            item.position = {pos.x, pos.y};
            item.zIndex = zIndex;
            item.type = RenderItem::Type::Circle;
            auto& circle = world->GetComponent<CircleComponent>(entity);
            item.data = CircleData{circle.radius, circle.background, circle.border};
            renderItems.push_back(item);
        }

        // Check for TextComponent
        if (world->HasComponent<TextComponent>(entity)) {
            RenderItem item;
            item.entity = entity;
            item.position = {pos.x, pos.y};
            item.zIndex = zIndex;
            item.type = RenderItem::Type::Text;
            auto& text = world->GetComponent<TextComponent>(entity);
            item.data = TextData{text.content, text.fontSize, text.color};
            renderItems.push_back(item);
        }
    });

    // Sort by z-index
    std::stable_sort(renderItems.begin(), renderItems.end(),
        [](const RenderItem& a, const RenderItem& b) {
            return a.zIndex < b.zIndex;
        });

    // Render sorted items
    for (const auto& item : renderItems) {
        if (item.type == RenderItem::Type::Rectangle) {
            const auto& rect = std::get<RectangleData>(item.data);
            float topLeftX = item.position.x - rect.width * 0.5f;
            float topLeftY = item.position.y - rect.height * 0.5f;
            DrawRectangleV({topLeftX, topLeftY},
                          {rect.width, rect.height},
                          rect.background);
            if (rect.border.a > 0) {
                DrawRectangleLines(topLeftX, topLeftY,
                                 rect.width,
                                 rect.height,
                                 rect.border);
            }
        } else if (item.type == RenderItem::Type::Circle) {
            const auto& circle = std::get<CircleData>(item.data);
            DrawCircleV({item.position.x, item.position.y},
                       circle.radius,
                       circle.background);
            if (circle.border.a > 0) {
                DrawCircleLinesV({item.position.x, item.position.y},
                               circle.radius,
                               circle.border);
            }
        } else if (item.type == RenderItem::Type::Text) {
            const auto& text = std::get<TextData>(item.data);
            int textWidth = MeasureText(text.content.c_str(), text.fontSize);
            int centeredX = item.position.x - textWidth * 0.5f;
            int centeredY = item.position.y - text.fontSize * 0.5f;
            DrawText(text.content.c_str(),
                    centeredX,
                    centeredY,
                    text.fontSize,
                    text.color);
        } else {
            throw std::runtime_error("Unknown render item type");
        }
    }
}

} // namespace Elysium::Systems
