#include "Systems/RenderSystem.h"
#include <algorithm>
#include <optional>
#include <stdexcept>
#include <variant>
#include <vector>
#include "Core/Application.h"
#include "Core/Entity.h"
#include "Core/Scene.h"
#include "Services/AssetService.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

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
        camera.viewport.height);

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
    for (const auto& [layerIndex, items] : layerItems) {
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

    // Draw debug bounds and selection highlights on top of everything with world space transform
    LayerComponent worldLayer;
    worldLayer.space = LayerComponent::Space::World;
    worldLayer.blend = LayerComponent::Blend::Normal;
    Matrix transform = GetLayerTransform(worldLayer, cameraEntity);
    rlPushMatrix();
    rlMultMatrixf(MatrixToFloat(transform));
    DrawSelectionHighlights();
    DrawHealthBars();
    DrawDebugBounds();
    rlPopMatrix();

    EndScissorMode();
}

void RenderSystem::DrawHealthBars() {
    world->Query<HealthComponent, PositionComponent>([&](Entity entity, auto& health, auto& pos) {
        // Find height to draw at (prefer above BoundsComponent if it exists)
        float topY = pos.y - 32.0f; // Default offset
        float barWidth = 40.0f;
        float barHeight = 5.0f;

        if (world->HasComponent<BoundsComponent>(entity)) {
            auto& bounds = world->GetComponent<BoundsComponent>(entity);
            if (bounds.bounds.width > 0) {
                topY = bounds.bounds.y - 10.0f;
                barWidth = bounds.bounds.width;
            }
        }

        // Clamp bar width to a reasonable size
        if (barWidth < 30.0f) barWidth = 30.0f;
        if (barWidth > 60.0f) barWidth = 60.0f;

        Vector2 barPos = {pos.x - barWidth * 0.5f, topY - barHeight};

        // Draw background
        DrawRectangleV(barPos, {barWidth, barHeight}, BLACK);

        // Draw health
        float healthPercent = health.current / health.max;
        if (healthPercent < 0) healthPercent = 0;
        if (healthPercent > 1) healthPercent = 1;

        Color healthColor = GREEN;
        if (healthPercent < 0.25f) healthColor = RED;
        else if (healthPercent < 0.5f) healthColor = ORANGE;

        DrawRectangleV(barPos, {barWidth * healthPercent, barHeight}, healthColor);
        
        // Draw border
        DrawRectangleLinesEx({barPos.x, barPos.y, barWidth, barHeight}, 1.0f, {50, 50, 50, 255});
    });
}

bool RenderSystem::CanCameraSeeLayer(Entity entity, const CameraComponent& camera, const std::string& layerName, const Layers& layers) {
    // For now, we'll assume all cameras can see all layers
    // TODO: Implement proper layer filtering based on layerName
    return true;
}

void RenderSystem::RenderLayer(int index, const std::vector<RenderItem>& items, Entity cameraEntity, const LayerComponent& layer) {
    ApplyBlendMode(layer.blend);

    Matrix transform = GetLayerTransform(layer, cameraEntity);

    rlPushMatrix();
    rlMultMatrixf(MatrixToFloat(transform));

    for (const auto& item : items) {
        RenderSingleItem(item, layer);
    }

    rlPopMatrix();
}

void RenderSystem::ApplyBlendMode(const LayerComponent::Blend& blend) {
    switch (blend) {
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

Matrix RenderSystem::GetLayerTransform(const LayerComponent& layer, Entity cameraEntity) {
    auto& cameraPosition = world->GetComponent<PositionComponent>(cameraEntity);
    auto& camera = world->GetComponent<CameraComponent>(cameraEntity);

    switch (layer.space) {
        case LayerComponent::Space::Screen: {
            return MatrixIdentity();
        }
        case LayerComponent::Space::World: {
            // Center the viewport - translate to center of camera viewport
            Vector2 viewportCenter = {
                camera.viewport.width * 0.5f,
                camera.viewport.height * 0.5f};

            Matrix centerTranslation = MatrixTranslate(viewportCenter.x, viewportCenter.y, 0);
            Matrix scale = MatrixScale(camera.zoom, camera.zoom, 1.0f);
            Matrix cameraTranslation = MatrixTranslate(-cameraPosition.x, -cameraPosition.y, 0);

            // Apply in order: camera translation -> scale -> center in viewport
            return MatrixMultiply(MatrixMultiply(cameraTranslation, scale), centerTranslation);
        }
        case LayerComponent::Space::Parallax: {
            Vector2 parallaxOffset = {
                -cameraPosition.x * layer.parallaxFactor.x,
                -cameraPosition.y * layer.parallaxFactor.y};

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
    if (world->HasComponent<TextureComponent>(entity)) {
        return world->GetComponent<TextureComponent>(entity);
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
    },
                      renderable.value());
}

void RenderSystem::RenderSingleItem(const RenderItem& item, const LayerComponent& layer) {
    // Compute bounds if entity has BoundsComponent
    ComputeBounds(item.entity, item);

    std::visit([&](const auto& component) {
        using T = std::decay_t<decltype(component)>;

        if constexpr (std::is_same_v<T, RectangleComponent>) {
            float topLeftX = item.position.x - component.width * 0.5f;
            float topLeftY = item.position.y - component.height * 0.5f;
            DrawRectangleV({topLeftX, topLeftY}, {component.width, component.height}, component.background);
            if (component.border.a > 0) {
                DrawRectangleLines(topLeftX, topLeftY, component.width, component.height, component.border);
            }
        } else if constexpr (std::is_same_v<T, CircleComponent>) {
            DrawCircleV({item.position.x, item.position.y}, component.radius, component.background);
            if (component.border.a > 0) {
                DrawCircleLinesV({item.position.x, item.position.y}, component.radius, component.border);
            }
        } else if constexpr (std::is_same_v<T, TextComponent>) {
            // Get scale (default to 1,1 if no ScaleComponent)
            float scaleX = 1.0f, scaleY = 1.0f;
            if (world->HasComponent<ScaleComponent>(item.entity)) {
                auto& scale = world->GetComponent<ScaleComponent>(item.entity);
                scaleX = scale.x;
                scaleY = scale.y;
            }

            // Apply scale to font size (use average of x and y scale for text)
            float avgScale = (scaleX + scaleY) / 2.0f;
            int scaledFontSize = component.fontSize * avgScale;

            int textWidth = MeasureText(component.content.c_str(), scaledFontSize);
            int centeredX = item.position.x - textWidth * 0.5f;
            int centeredY = item.position.y - scaledFontSize * 0.5f;
            DrawText(component.content.c_str(), centeredX, centeredY, scaledFontSize, component.color);
        } else if constexpr (std::is_same_v<T, LightComponent>) {
            DrawCircleV({item.position.x, item.position.y}, component.radius, component.color);
        } else if constexpr (std::is_same_v<T, SpriteComponent>) {
            const Sprite& sprite = component.sprite;
            const std::string& marker = component.markerName;

            // Use GetMarkerFrameClip to get the specific frame within the marker
            Rectangle sourceRect = sprite.GetMarkerFrameClip(marker, component.frameIndex);
            std::string textureName = sprite.GetMarkerTextureName(marker);

            if (!textureName.empty() && sourceRect.width > 0 && sourceRect.height > 0) {
                auto& assets = Application::GetInstance().GetService<Elysium::Services::AssetService>();
                Texture2D texture = assets.GetTexture(textureName);

                if (texture.id != 0) {
                    // Get scale (default to 1,1 if no ScaleComponent)
                    float scaleX = 1.0f, scaleY = 1.0f;
                    if (world->HasComponent<ScaleComponent>(item.entity)) {
                        auto& scale = world->GetComponent<ScaleComponent>(item.entity);
                        scaleX = scale.x;
                        scaleY = scale.y;
                    }

                    // Apply scale to destination size
                    float scaledWidth = sourceRect.width * scaleX;
                    float scaledHeight = sourceRect.height * scaleY;

                    Rectangle destRect = {
                        item.position.x - scaledWidth * 0.5f,
                        item.position.y - scaledHeight * 0.5f,
                        scaledWidth,
                        scaledHeight};
                    Vector2 origin = {0, 0};
                    DrawTexturePro(texture, sourceRect, destRect, origin, 0.0f, WHITE);
                }
            }
        } else if constexpr (std::is_same_v<T, TextureComponent>) {
            if (!component.textureName.empty()) {
                auto& assets = Application::GetInstance().GetService<Elysium::Services::AssetService>();
                Texture2D texture = assets.GetTexture(component.textureName);

                if (texture.id != 0) {
                    // Source rect: use clip if specified, otherwise full texture
                    Rectangle sourceRect = component.clip;
                    if (sourceRect.width <= 0 || sourceRect.height <= 0) {
                        sourceRect = {0, 0, (float)texture.width, (float)texture.height};
                    }

                    // Get scale (default to 1,1 if no ScaleComponent)
                    float scaleX = 1.0f, scaleY = 1.0f;
                    if (world->HasComponent<ScaleComponent>(item.entity)) {
                        auto& scale = world->GetComponent<ScaleComponent>(item.entity);
                        scaleX = scale.x;
                        scaleY = scale.y;
                    }

                    // Apply scale to destination size
                    float scaledWidth = sourceRect.width * scaleX;
                    float scaledHeight = sourceRect.height * scaleY;

                    Rectangle destRect = {
                        item.position.x - scaledWidth * 0.5f,
                        item.position.y - scaledHeight * 0.5f,
                        scaledWidth,
                        scaledHeight};

                    Vector2 origin = {0, 0};
                    DrawTexturePro(texture, sourceRect, destRect, origin, 0.0f, component.tint);
                }
            }
        }
    },
               item.renderable);
}

void RenderSystem::ComputeBounds(Entity entity, const RenderItem& item) {
    // Only compute bounds if the entity has a BoundsComponent
    if (!world->HasComponent<BoundsComponent>(entity)) {
        return;
    }

    auto& bounds = world->GetComponent<BoundsComponent>(entity);

    // Compute bounding rectangle based on renderable type
    std::visit([&](const auto& component) {
        using T = std::decay_t<decltype(component)>;

        if constexpr (std::is_same_v<T, RectangleComponent>) {
            float halfWidth = component.width * 0.5f;
            float halfHeight = component.height * 0.5f;
            bounds.bounds = {
                item.position.x - halfWidth,
                item.position.y - halfHeight,
                component.width,
                component.height};
        } else if constexpr (std::is_same_v<T, CircleComponent>) {
            bounds.bounds = {
                item.position.x - component.radius,
                item.position.y - component.radius,
                component.radius * 2,
                component.radius * 2};
        } else if constexpr (std::is_same_v<T, SpriteComponent>) {
            const Sprite& sprite = component.sprite;
            const std::string& marker = component.markerName;
            Rectangle sourceRect = sprite.GetMarkerFrameClip(marker, component.frameIndex);

            // Get scale (default to 1,1 if no ScaleComponent)
            float scaleX = 1.0f, scaleY = 1.0f;
            if (world->HasComponent<ScaleComponent>(entity)) {
                auto& scale = world->GetComponent<ScaleComponent>(entity);
                scaleX = scale.x;
                scaleY = scale.y;
            }

            float scaledWidth = sourceRect.width * scaleX;
            float scaledHeight = sourceRect.height * scaleY;

            bounds.bounds = {
                item.position.x - scaledWidth * 0.5f,
                item.position.y - scaledHeight * 0.5f,
                scaledWidth,
                scaledHeight};
        } else if constexpr (std::is_same_v<T, TextureComponent>) {
            if (!component.textureName.empty()) {
                auto& assets = Application::GetInstance().GetService<Elysium::Services::AssetService>();
                Texture2D texture = assets.GetTexture(component.textureName);

                if (texture.id != 0) {
                    // Source rect: use clip if specified, otherwise full texture
                    Rectangle sourceRect = component.clip;
                    if (sourceRect.width <= 0 || sourceRect.height <= 0) {
                        sourceRect = {0, 0, (float)texture.width, (float)texture.height};
                    }

                    // Get scale (default to 1,1 if no ScaleComponent)
                    float scaleX = 1.0f, scaleY = 1.0f;
                    if (world->HasComponent<ScaleComponent>(entity)) {
                        auto& scale = world->GetComponent<ScaleComponent>(entity);
                        scaleX = scale.x;
                        scaleY = scale.y;
                    }

                    float scaledWidth = sourceRect.width * scaleX;
                    float scaledHeight = sourceRect.height * scaleY;

                    bounds.bounds = {
                        item.position.x - scaledWidth * 0.5f,
                        item.position.y - scaledHeight * 0.5f,
                        scaledWidth,
                        scaledHeight};
                }
            }
        } else if constexpr (std::is_same_v<T, TextComponent>) {
            // Get scale (default to 1,1 if no ScaleComponent)
            float scaleX = 1.0f, scaleY = 1.0f;
            if (world->HasComponent<ScaleComponent>(entity)) {
                auto& scale = world->GetComponent<ScaleComponent>(entity);
                scaleX = scale.x;
                scaleY = scale.y;
            }

            // Apply scale to font size (use average of x and y scale for text)
            float avgScale = (scaleX + scaleY) / 2.0f;
            int scaledFontSize = component.fontSize * avgScale;

            int textWidth = MeasureText(component.content.c_str(), scaledFontSize);
            bounds.bounds = {
                item.position.x - textWidth * 0.5f,
                item.position.y - scaledFontSize * 0.5f,
                (float)textWidth,
                (float)scaledFontSize};
        } else if constexpr (std::is_same_v<T, LightComponent>) {
            bounds.bounds = {
                item.position.x - component.radius,
                item.position.y - component.radius,
                component.radius * 2,
                component.radius * 2};
        }
    },
               item.renderable);
}

void RenderSystem::DrawDebugBounds() {
    // Get scene to check debug flag
    Scene* scene = GetScene();
    if (!scene)
        return;

    // TODO: Add debug flag check to scene once implemented
    // For now, always draw debug bounds if entity has BoundsComponent

    world->Query<BoundsComponent>([&](Entity entity, auto& bounds) {
        // Skip if bounds haven't been computed yet (width/height are 0)
        if (bounds.bounds.width <= 0 || bounds.bounds.height <= 0) {
            return;
        }

        // Draw the bounding box
        DrawRectangleLines(
            bounds.bounds.x,
            bounds.bounds.y,
            bounds.bounds.width,
            bounds.bounds.height,
            bounds.debugColor);

        // If dragging, draw a filled semi-transparent overlay
        if (bounds.isDragging) {
            Color dragColor = bounds.debugColor;
            dragColor.a = 50;
            DrawRectangleRec(bounds.bounds, dragColor);
        }
    });
}

void RenderSystem::DrawSelectionHighlights() {
    world->Query<SelectionComponent, BoundsComponent>([&](Entity entity, auto& sel, auto& bounds) {
        // Skip if bounds haven't been computed yet
        if (bounds.bounds.width <= 0 || bounds.bounds.height <= 0) {
            return;
        }

        // Draw selection highlight - green border with slight padding
        const float padding = 2.0f;
        Rectangle highlightRect = {
            bounds.bounds.x - padding,
            bounds.bounds.y - padding,
            bounds.bounds.width + padding * 2,
            bounds.bounds.height + padding * 2
        };

        // Draw thick green border for selection
        DrawRectangleLinesEx(highlightRect, 2.0f, GREEN);

        // Draw corner markers for extra visibility
        const float cornerSize = 6.0f;
        Color cornerColor = {0, 255, 0, 200};

        // Top-left
        DrawLineEx({highlightRect.x, highlightRect.y},
                   {highlightRect.x + cornerSize, highlightRect.y}, 2.0f, cornerColor);
        DrawLineEx({highlightRect.x, highlightRect.y},
                   {highlightRect.x, highlightRect.y + cornerSize}, 2.0f, cornerColor);

        // Top-right
        DrawLineEx({highlightRect.x + highlightRect.width, highlightRect.y},
                   {highlightRect.x + highlightRect.width - cornerSize, highlightRect.y}, 2.0f, cornerColor);
        DrawLineEx({highlightRect.x + highlightRect.width, highlightRect.y},
                   {highlightRect.x + highlightRect.width, highlightRect.y + cornerSize}, 2.0f, cornerColor);

        // Bottom-left
        DrawLineEx({highlightRect.x, highlightRect.y + highlightRect.height},
                   {highlightRect.x + cornerSize, highlightRect.y + highlightRect.height}, 2.0f, cornerColor);
        DrawLineEx({highlightRect.x, highlightRect.y + highlightRect.height},
                   {highlightRect.x, highlightRect.y + highlightRect.height - cornerSize}, 2.0f, cornerColor);

        // Bottom-right
        DrawLineEx({highlightRect.x + highlightRect.width, highlightRect.y + highlightRect.height},
                   {highlightRect.x + highlightRect.width - cornerSize, highlightRect.y + highlightRect.height}, 2.0f, cornerColor);
        DrawLineEx({highlightRect.x + highlightRect.width, highlightRect.y + highlightRect.height},
                   {highlightRect.x + highlightRect.width, highlightRect.y + highlightRect.height - cornerSize}, 2.0f, cornerColor);
    });
}

}  // namespace Elysium::Systems
