#include "Systems/RenderSystem.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "Application.h"
#include "Scene.h"
#include "Entity.h"
#include "Services/AssetService.h"
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <variant>
#include <optional>
namespace Elysium::Systems {

void RenderSystem::Draw() {
    // Find the first active camera
    Entity cameraEntity;
    CameraComponent* camera = nullptr;
    bool foundCamera = false;

    world->Query<CameraComponent>([&](Entity entity, auto& cameraComp) {
        if (!foundCamera) {
            if (cameraComp.isVisible) {
                cameraEntity = entity;
                camera = &cameraComp;
                foundCamera = true;
            }
        }
    });

    if (!foundCamera || !camera) {
        ClearBackground(BLACK);
        DrawText("No active camera found", 10, 10, 20, RED);
        return;
        // throw std::runtime_error("No active camera found. Scene must have at least one visible CameraComponent.");
    }

    // Build layer definition lookup = layerIndex -> <Entity, LayerComponent>
    Layers layers;
    world->Query<LayerComponent>([&](Entity entity, auto& layer) {
        if (layer.isVisible) {
            int layerIndex = layer.zIndex;
            layers[layerIndex] = {entity, &layer};
        }
    });

    // Render the single camera view
    RenderCamera(cameraEntity, *camera, layers);
}

void RenderSystem::RenderCamera(Entity cameraEntity, const CameraComponent& camera, const Layers& layers) {
    // Get the camera's world transform
    Vector2 cameraWorldPosition = {0, 0};
    if (world->HasComponent<PositionComponent>(cameraEntity)) {
        auto& pos = world->GetComponent<PositionComponent>(cameraEntity);
        cameraWorldPosition.x = pos.x;
        cameraWorldPosition.y = pos.y;
    }

    BeginScissorMode(
        camera.viewport.x,
        camera.viewport.y,
        camera.viewport.width,
        camera.viewport.height
    );

    // Collects and group renderable entities by layer;
    std::unordered_map<int, std::vector<RenderItem>> layerItems;

    world->Query<PositionComponent>([&](Entity entity, auto& pos) {
        // Skip camera and layer definition entities (they don't render as game objects)
        if (world->HasComponent<CameraComponent>(entity) ||
            world->HasComponent<LayerComponent>(entity)) {
            return;
        }

        // Get layer index - default to 0 if no LayerComponent
        int layerIndex = 0;
        if (world->HasComponent<LayerComponent>(entity)) {
            layerIndex = world->GetComponent<LayerComponent>(entity).zIndex;
        }

        std::optional<Renderable> renderable = GetRenderable(entity);
        if (!renderable.has_value()) {
            return;
        }

        std::string layerName = GetLayerName(renderable);

        // Check if this camera can see this layer
        if (!CanCameraSeeLayer(entity, camera, layerName, layers)) {
            return;
        }

        // Create render item and add to layer
        RenderItem item;
        item.entity = entity;
        item.position = {pos.x, pos.y};
        item.renderable = renderable.value();

        layerItems[layerIndex].push_back(item);
    });

    // Render layers in order
    std::vector<int> sortedLayers;
    for (const auto& [layerIndex, items]: layerItems) {
        sortedLayers.push_back(layerIndex);
    }
    std::sort(sortedLayers.begin(), sortedLayers.end());

    for (int layerIndex : sortedLayers) {
        auto layerIt = layers.find(layerIndex);
        if (layerIt != layers.end()) {
            RenderLayer(layerIndex, layerItems[layerIndex],
                cameraEntity, *layerIt->second.second);
        } else {
            LayerComponent defaultLayer;
            RenderLayer(layerIndex, layerItems[layerIndex], cameraEntity, defaultLayer);
        }
    }

    EndScissorMode();
}

bool RenderSystem::CanCameraSeeLayer(Entity entity, const CameraComponent& camera, const std::string& layerName, const Layers& layers) {
    // For now, we'll assume all cameras can see all layers
    // TODO: Implement proper layer filtering based on layerName
    return true;
}

void RenderSystem::RenderLayer(int index, const std::vector<RenderItem>& items, Entity cameraEntity, const LayerComponent& layer)
{
    ApplyBlendMode(layer.blend);

    Matrix transform = GetLayerTransform(layer, cameraEntity);

    rlPushMatrix();
    rlMultMatrixf(MatrixToFloat(transform));

    for (const auto& item : items) {
        RenderSingleItem(item, layer);
    }

    rlPopMatrix();
}

void RenderSystem::ApplyBlendMode(const LayerComponent::Blend& blend)
{
    switch (blend)
    {
    case LayerComponent::Blend::Normal:
        BeginBlendMode(BLEND_ALPHA);
        break;
    case LayerComponent::Blend::Additive:
        BeginBlendMode(BLEND_ADDITIVE);
        break;
    case LayerComponent::Blend::Multiply:
        BeginBlendMode(BLEND_MULTIPLIED);
        break;
    case LayerComponent::Blend::Alpha:
        BeginBlendMode(BLEND_ALPHA);
        break;
    default:
        BeginBlendMode(BLEND_ALPHA);
        break;
    }
}

Matrix RenderSystem::GetLayerTransform(const LayerComponent& layer, Entity cameraEntity)
{
    auto &cameraPosition = world->GetComponent<PositionComponent>(cameraEntity);
    auto &camera = world->GetComponent<CameraComponent>(cameraEntity);



    switch (layer.space) {
        case LayerComponent::Space::Screen: {
            return MatrixIdentity();
        }
        case LayerComponent::Space::World: {
            // Center the viewport - translate to center of camera viewport
            Vector2 viewportCenter = {
                camera.viewport.width * 0.5f,
                camera.viewport.height * 0.5f
            };

            Matrix centerTranslation = MatrixTranslate(viewportCenter.x, viewportCenter.y, 0);
            Matrix scale = MatrixScale(camera.zoom, camera.zoom, 1.0f);
            Matrix cameraTranslation = MatrixTranslate(-cameraPosition.x, -cameraPosition.y, 0);

            // Apply in order: camera translation -> scale -> center in viewport
            return MatrixMultiply(MatrixMultiply(cameraTranslation, scale), centerTranslation);
        }
        case LayerComponent::Space::Parallax: {
            Vector2 parallaxOffset = {
                -cameraPosition.x * layer.parallaxFactor.x,
                -cameraPosition.y * layer.parallaxFactor.y
            };

            Matrix translation = MatrixTranslate(parallaxOffset.x, parallaxOffset.y, 0);

            float parallaxZoom = 1.0f + (camera.zoom - 1.0f) * (layer.parallaxFactor.x + layer.parallaxFactor.y) * 0.5f;

            Matrix scale = MatrixScale(parallaxZoom, parallaxZoom, 1.0f);

            return MatrixMultiply(translation, scale);
        }
    }

    return MatrixIdentity();
}

std::optional<Renderable> RenderSystem::GetRenderable(Entity entity) {
    if (world->HasComponent<RectangleComponent>(entity)) {
        return world->GetComponent<RectangleComponent>(entity);
    }
    if (world->HasComponent<CircleComponent>(entity)) {
        return world->GetComponent<CircleComponent>(entity);
    }
    if (world->HasComponent<TextComponent>(entity)) {
        return world->GetComponent<TextComponent>(entity);
    }
    if (world->HasComponent<SpriteComponent>(entity)) {
        return world->GetComponent<SpriteComponent>(entity);
    }
    if (world->HasComponent<LightComponent>(entity)) {
        return world->GetComponent<LightComponent>(entity);
    }
    return std::nullopt;
}

std::string RenderSystem::GetLayerName(const std::optional<Renderable>& renderable) {
    if (!renderable.has_value()) {
        return "default";
    }

    return std::visit([](const auto& component) -> std::string {
        return component.layerName;
    }, renderable.value());
}

void RenderSystem::RenderSingleItem(const RenderItem& item, const LayerComponent& layer) {
    std::visit([&](const auto& component) {
        using T = std::decay_t<decltype(component)>;

        if constexpr (std::is_same_v<T, RectangleComponent>) {
            float topLeftX = item.position.x - component.width * 0.5f;
            float topLeftY = item.position.y - component.height * 0.5f;
            DrawRectangleV({topLeftX, topLeftY}, {component.width, component.height}, component.background);
            if (component.border.a > 0) {
                DrawRectangleLines(topLeftX, topLeftY, component.width, component.height, component.border);
            }
        }
        else if constexpr (std::is_same_v<T, CircleComponent>) {
            DrawCircleV({item.position.x, item.position.y}, component.radius, component.background);
            if (component.border.a > 0) {
                DrawCircleLinesV({item.position.x, item.position.y}, component.radius, component.border);
            }
        }
        else if constexpr (std::is_same_v<T, TextComponent>) {
            int textWidth = MeasureText(component.content.c_str(), component.fontSize);
            int centeredX = item.position.x - textWidth * 0.5f;
            int centeredY = item.position.y - component.fontSize * 0.5f;
            DrawText(component.content.c_str(), centeredX, centeredY, component.fontSize, component.color);
        }
        else if constexpr (std::is_same_v<T, LightComponent>) {
            DrawCircleV({item.position.x, item.position.y}, component.radius, component.color);
        }
        else if constexpr (std::is_same_v<T, SpriteComponent>) {
            const Sprite& sprite = component.sprite;
            const std::string& marker = component.markerName;

            // Use GetMarkerFrameClip to get the specific frame within the marker
            Rectangle sourceRect = sprite.GetMarkerFrameClip(marker, component.frameIndex);
            std::string textureName = sprite.GetMarkerTextureName(marker);

            if (!textureName.empty() && sourceRect.width > 0 && sourceRect.height > 0) {
                auto& assets = Application::GetInstance().GetService<Elysium::Services::AssetService>("AssetService");
                Texture2D texture = assets.GetTexture(textureName);

                if (texture.id != 0) {

                    // For grid alignment, we want sprites to be centered on their tile position
                    // Since sprites might be larger than tiles, we center them properly
                    Rectangle destRect = {
                        item.position.x - sourceRect.width * 0.5f,
                        item.position.y - sourceRect.height * 0.5f,
                        sourceRect.width,
                        sourceRect.height
                    };
                    Vector2 origin = {0, 0};
                    DrawTexturePro(texture, sourceRect, destRect, origin, 0.0f, WHITE);
                }
            }
        }
    }, item.renderable);
}
} // namespace Elysium::Systems
