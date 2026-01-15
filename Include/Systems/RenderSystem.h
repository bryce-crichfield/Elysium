#pragma once

#include "System.h"
#include "Component.h"
#include "raylib.h"
#include <unordered_map>
#include <vector>
#include <optional>
#include <variant>
#include <string>

namespace Elysium::Systems {

struct RenderItem {
    Entity entity;
    Vector2 position;
    std::variant<RectangleComponent, CircleComponent, TextComponent, SpriteComponent, LightComponent> renderable;
};

using Renderable = std::variant<RectangleComponent, CircleComponent, TextComponent, SpriteComponent, LightComponent>;

class RenderSystem : public System {
public:
    using Layers = std::unordered_map<int, std::pair<Entity, LayerComponent*>>;
    using CameraEntity = std::pair<Entity, CameraComponent>;

    RenderSystem(Context context) : System(context) {}

    void Draw() override;
private:
    void RenderCamera(Entity entity, const CameraComponent& camera, const Layers& layers);
    bool CanCameraSeeLayer(Entity entity, const CameraComponent& camera, const std::string& layerName, const Layers& layers);
    void RenderLayer(int index, const std::vector<RenderItem>& items, Entity cameraEntity, const LayerComponent& layer);
    std::optional<Renderable> GetRenderable(Entity entity);
    std::string GetLayerName(const std::optional<Renderable>& renderable);
    void RenderSingleItem(const RenderItem& item, const LayerComponent& layer);
    void ApplyBlendMode(const LayerComponent::Blend& blend);
    Matrix GetLayerTransform(const LayerComponent& layer, Entity cameraEntity);
    void ComputeBounds(Entity entity, const RenderItem& item);
    void DrawDebugBounds();
};

} // namespace Elysium::Systems
