#include "Systems/RenderSystem.h"
#include "Core/SystemRegistry.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <variant>
#include <vector>
#include "Core/Common.h"
#include "Core/Application.h"
#include "Core/Path.h"
#include "Core/Entity.h"
#include "Core/Scene.h"
#include "Core/RenderContext.h"
#include "Services/AssetService.h"
#include "Services/SceneService.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "Components/CameraComponent.h"
#include "Components/LayerComponent.h"
#include "Components/PositionComponent.h"
#include "Components/RectangleComponent.h"
#include "Components/CircleComponent.h"
#include "Components/TextComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/LightComponent.h"
#include "Components/BoundsComponent.h"
#include "Components/ScaleComponent.h"
#include "Components/HealthComponent.h"
#include "Components/SelectionComponent.h"
#include "Components/DebugComponent.h"
#include "Components/NameComponent.h"
#include "Components/TileComponent.h"

namespace Elysium::Systems {

RenderSystem::~RenderSystem() {
    if (_lightMap.id != 0) {
        UnloadRenderTexture(_lightMap);
    }
}

RenderTexture2D& RenderSystem::EnsureLightMap(int width, int height) {
    if (_lightMap.id == 0 || _lightMapWidth != width || _lightMapHeight != height) {
        if (_lightMap.id != 0) {
            UnloadRenderTexture(_lightMap);
        }
        _lightMap = LoadRenderTexture(width, height);
        _lightMapWidth = width;
        _lightMapHeight = height;
    }
    return _lightMap;
}

void RenderSystem::Draw() {
    FindCameras();
    RenderContext ctx;
    
    for (auto& cameraEntity : _cameraEntities) {
        RenderView(ctx, cameraEntity);
    }
}

void RenderSystem::FindCameras() {
    _cameraEntities.clear();

    world->Query<CameraComponent>([&](Entity entity, auto& cameraComp) {
        _cameraEntities.push_back(entity);
    });

    if (_cameraEntities.empty()) {
        ClearBackground(BLACK);
        DrawText("No active camera found", 10, 10, 20, RED);
        return;
    }

    std::sort(_cameraEntities.begin(), _cameraEntities.end(), [&](Entity a, Entity b) {
        auto& camA = world->GetComponent<CameraComponent>(a);
        auto& camB = world->GetComponent<CameraComponent>(b);
        return camA.renderOrder < camB.renderOrder;
    });
}

void RenderSystem::RenderView(RenderContext& ctx, Entity cameraEntity) {
    auto& camera = world->GetComponent<CameraComponent>(cameraEntity);
    if (!camera.isVisible)
        return;

    ctx.PushScissorMode(
        (int)camera.viewport.x,
        (int)camera.viewport.y,
        (int)camera.viewport.width,
        (int)camera.viewport.height);

    // Collect and group renderable entities by layer name
    std::unordered_map<std::string, std::vector<RenderableObject>> layerItems;
    std::vector<RenderableObject> debugWorld2D;
    std::vector<RenderableObject> debugScreen2D;

    world->Query<PositionComponent>([&](Entity entity, auto& pos) {
        if (world->HasComponent<CameraComponent>(entity)) {
            return;
        }

        std::vector<Renderable> renderable = GetRenderables(entity);
        if (renderable.empty()) {
            return;
        }

        std::string layerName = GetLayerName(entity);

        for (auto& renderableComp : renderable) {
            RenderableObject item;
            item.entity = entity;
            item.position = {pos.x, pos.y};
            item.renderable = renderableComp;

            // Intercept debug components
            if (std::holds_alternative<DebugComponent>(renderableComp)) {
                // Determine which debug layer based on entity's actual layer space
                auto actualLayer = scene->GetLayer(layerName);
                if (actualLayer && actualLayer->space == SceneLayerSpace::Screen2D) {
                    debugScreen2D.push_back(item);
                } else {
                    debugWorld2D.push_back(item);
                }
            } else {
                layerItems[layerName].push_back(item);
            }
        }
    });
     
    std::vector<SceneLayer> layers = scene->GetLayers();

    // Render layers in their defined order (by zIndex/order in list)
    for (const auto& layer : layers) {
        if (!layer.isVisible)
            continue;

        auto it = layerItems.find(layer.name);
        if (it != layerItems.end()) {
            if (layer.isComposited) {
                RenderCompositedLayer(ctx, cameraEntity, layer, it->second);
            } else {
                RenderImmediateLayer(ctx, cameraEntity, layer, it->second);
            }
        }
    }

    // Render debug overlays AFTER everything else
    if (!debugWorld2D.empty()) {
        SceneLayer debugLayerWorld;
        debugLayerWorld.name = "__debug_world2d__";
        debugLayerWorld.space = SceneLayerSpace::World2D;
        debugLayerWorld.layerBlend = SceneLayerBlend::Alpha;
        debugLayerWorld.isComposited = false;
        debugLayerWorld.isVisible = true;
        RenderImmediateLayer(ctx, cameraEntity, debugLayerWorld, debugWorld2D);
    }

    if (!debugScreen2D.empty()) {
        SceneLayer debugLayerScreen;
        debugLayerScreen.name = "__debug_screen2d__";
        debugLayerScreen.space = SceneLayerSpace::Screen2D;
        debugLayerScreen.layerBlend = SceneLayerBlend::Alpha;
        debugLayerScreen.isComposited = false;
        debugLayerScreen.isVisible = true;
        RenderImmediateLayer(ctx, cameraEntity, debugLayerScreen, debugScreen2D);
    }
    
    ctx.PopScissorMode();
}

void RenderSystem::RenderImmediateLayer(RenderContext& ctx, Entity cameraEntity, const SceneLayer& layer, const std::vector<RenderableObject>& objects) {
    ProfileN("Render Immediate Layer");
    Matrix viewProjectionTransform = CalculateTransform(cameraEntity, layer);
    PushBlendMode(ctx, layer.layerBlend);
    ctx.PushMatrix();
    ctx.MultiplyMatrix(viewProjectionTransform);

    for (const auto& object : objects) {
        RenderObject(ctx, object, layer);
    }

    ctx.PopMatrix();
    ctx.PopBlendMode();
}

// Composited layers are those that render to an offscreen texture first (e.g. light layers)
// This allows for blend modes to work correctly over the entire layer
void RenderSystem::RenderCompositedLayer(RenderContext& ctx, Entity cameraEntity, const SceneLayer& layer, const std::vector<RenderableObject>& objects) {
    ProfileN("Render Composited Layer");
    
    auto& camera = world->GetComponent<CameraComponent>(cameraEntity);
    Matrix layerTransform = CalculateTransform(cameraEntity, layer);

    int w = (int)camera.viewport.width;
    int h = (int)camera.viewport.height;
    RenderTexture2D& compositionBuffer = EnsureLightMap(w, h);

    ctx.PopScissorMode();

    ctx.BeginTextureMode(compositionBuffer);
    ClearBackground(layer.ambient);
    
    //ctx.PushBlendMode(BLEND_ADDITIVE);
    PushBlendMode(ctx, layer.layerBlend);
    ctx.PushMatrix();
    ctx.MultiplyMatrix(layerTransform);

    for (const auto& object : objects) {
        RenderObject(ctx, object, layer);
    }

    ctx.PopMatrix();
    ctx.PopBlendMode();
    
    ctx.EndTextureMode();

    // Restore SceneService's framebuffer
    auto& sceneService = Application::GetInstance().GetService<Services::SceneService>();
    ctx.BeginTextureMode(sceneService.GetFramebuffer());

    // Composite lightmap onto scene using layer's blend mode
    //PushBlendMode(ctx, layer.blend);
    PushBlendMode(ctx, layer.compositeBlend);
    Rectangle src = {0, 0, (float)w, -(float)h};
    Rectangle dst = {camera.viewport.x, camera.viewport.y, (float)w, (float)h};
    ctx.DrawTexturePro(compositionBuffer.texture, src, dst, {0, 0}, 0.0f, WHITE);
    
    ctx.PopBlendMode();
    
    // Restore scissor mode for subsequent layers
    ctx.PushScissorMode(
        (int)camera.viewport.x,
        (int)camera.viewport.y,
        (int)camera.viewport.width,
        (int)camera.viewport.height);
}

void RenderSystem::RenderObject(RenderContext& ctx, const RenderableObject& object, const SceneLayer& layer) {
    std::visit([&](const auto& component) {
        using T = std::decay_t<decltype(component)>;

        if constexpr (std::is_same_v<T, RectangleComponent>) {
            // Check if this is an isometric tile
            bool isIsometric = false;
            float tileWidth = component.width;
            float tileHeight = component.height;

            if (world->HasComponent<TileComponent>(object.entity)) {
                const auto& tile = world->GetComponent<TileComponent>(object.entity);
                isIsometric = tile.isIsometric;
                tileWidth = tile.tileWidth;
                tileHeight = tile.tileHeight;
            }

            if (isIsometric) {
                // Draw isometric diamond shape
                float cx = object.position.x;
                float cy = object.position.y;
                float hw = tileWidth * 0.5f;   // half width
                float hh = tileHeight * 0.5f;  // half height

                // Diamond vertices: top, right, bottom, left
                Vector2 top = {cx, cy - hh};
                Vector2 right = {cx + hw, cy};
                Vector2 bottom = {cx, cy + hh};
                Vector2 left = {cx - hw, cy};

                // Draw filled diamond as two triangles
                DrawTriangle(top, left, bottom, component.background);
                DrawTriangle(top, bottom, right, component.background);

                // Draw border lines
                if (component.border.a > 0) {
                    DrawLineV(top, right, component.border);
                    DrawLineV(right, bottom, component.border);
                    DrawLineV(bottom, left, component.border);
                    DrawLineV(left, top, component.border);
                }
            } else {
                // Standard rectangle rendering
                float topLeftX = object.position.x - component.width * 0.5f;
                float topLeftY = object.position.y - component.height * 0.5f;
                ctx.DrawRectangle(topLeftX, topLeftY, component.width, component.height, component.background);
                if (component.border.a > 0) {
                    ctx.DrawRectangleLines(topLeftX, topLeftY, component.width, component.height, component.border);
                }
            }
        } else if constexpr (std::is_same_v<T, CircleComponent>) {
            ctx.DrawCircle(object.position.x, object.position.y, component.radius, component.background);
            if (component.border.a > 0) {
                ctx.DrawCircleLines(object.position.x, object.position.y, component.radius, component.border);
            }
        } else if constexpr (std::is_same_v<T, TextComponent>) {
            float scaleX = 1.0f, scaleY = 1.0f;
            if (world->HasComponent<ScaleComponent>(object.entity)) {
                auto& scale = world->GetComponent<ScaleComponent>(object.entity);
                scaleX = scale.x;
                scaleY = scale.y;
            }

            float avgScale = (scaleX + scaleY) / 2.0f;
            int scaledFontSize = (int)(component.fontSize * avgScale);

            int textWidth = MeasureText(component.content.c_str(), scaledFontSize);
            int centeredX = (int)(object.position.x - textWidth * 0.5f);
            int centeredY = (int)(object.position.y - scaledFontSize * 0.5f);
            ctx.DrawText(component.content.c_str(), (float)centeredX, (float)centeredY, scaledFontSize, component.color);
        } else if constexpr (std::is_same_v<T, LightComponent>) {
            const int layers = 8;

            for (int i = 0; i < layers; i++) {
                float t = (float)i / layers;
                float power = 1.0f + component.intensity * 4.0f;
                float curve = powf(1.0f - t, power);
                float layerRadius = component.radius * curve;

                Color layerColor = component.color;
                layerColor.a = (unsigned char)(component.color.a / layers);
                Color layerEdge = {layerColor.r, layerColor.g, layerColor.b, 0};

                ctx.DrawCircleGradient(object.position.x, object.position.y, layerRadius, layerColor, layerEdge);
            }
        } else if constexpr (std::is_same_v<T, SpriteComponent>) {
            auto& assets = Application::GetInstance().GetService<Elysium::Services::AssetService>();

            Sprite sprite = assets.GetSprite(Path(component.spriteName));
            if (sprite.name.empty()) {
                return;
            }

            auto sheetIt = sprite.sheets.find(component.sheetName);
            if (sheetIt == sprite.sheets.end()) {
                return;
            }
            const SpriteSheet& sheet = sheetIt->second;

            auto seqIt = sheet.sequences.find(component.sequenceName);
            if (seqIt == sheet.sequences.end()) {
                return;
            }
            const SpriteSequence& sequence = seqIt->second;

            if (sequence.indices.empty()) {
                return;
            }

            size_t frameIdx = component.sequenceIndex % sequence.indices.size();
            size_t linearIndex = sequence.indices[frameIdx];

            Texture2D texture = assets.GetTexture(Path("Sprites/" + sheet.path));
            if (texture.id == 0) {
                return;
            }

            float frameWidth = (float)texture.width / (float)sheet.cols;
            float frameHeight = (float)texture.height / (float)sheet.rows;

            size_t col = linearIndex % sheet.cols;
            size_t row = linearIndex / sheet.cols;

            Rectangle sourceRect = {
                col * frameWidth,
                row * frameHeight,
                frameWidth,
                frameHeight
            };

            float scaleX = 1.0f, scaleY = 1.0f;
            if (world->HasComponent<ScaleComponent>(object.entity)) {
                auto& scale = world->GetComponent<ScaleComponent>(object.entity);
                scaleX = scale.x;
                scaleY = scale.y;
            }

            float scaledWidth = frameWidth * scaleX;
            float scaledHeight = frameHeight * scaleY;

            Rectangle destRect = {
                object.position.x - scaledWidth * 0.5f,
                object.position.y - scaledHeight * 0.5f,
                scaledWidth,
                scaledHeight
            };
            Vector2 origin = {0, 0};
            ctx.DrawTexturePro(texture, sourceRect, destRect, origin, 0.0f, WHITE);
        } else if constexpr (std::is_same_v<T, DebugComponent>) {
            const DebugComponent& debug = component;

            if (!debug.isSelected) {
                return;
            }

            auto aabbMin = debug.aabbMin;
            auto aabbMax = debug.aabbMax;

            ctx.DrawRectangleLinesEx(
                Rectangle{
                    aabbMin.x,
                    aabbMin.y,
                    aabbMax.x - aabbMin.x,
                    aabbMax.y - aabbMin.y},
                2.0f,
                debug.isSelected ? debug.highlightColor : RED);

            // Draw dog ear name tag if entity has a name
            if (world->HasComponent<NameComponent>(object.entity)) {
                auto& name = world->GetComponent<NameComponent>(object.entity);

                int fontSize = 10;
                int textWidth = MeasureText(name.name.c_str(), fontSize);

                // Dog ear tab positioned at top-right corner
                float tabWidth = textWidth + 6.0f;
                float tabHeight = fontSize + 4.0f;
                float tabX = aabbMax.x - tabWidth;
                float tabY = aabbMin.y - tabHeight;

                Color earColor = debug.isSelected ? debug.highlightColor : RED;

                // Draw tab background
                ctx.DrawRectangle(tabX, tabY, tabWidth, tabHeight, earColor);

                // Draw text
                ctx.DrawText(name.name.c_str(), tabX + 3, tabY + 2, fontSize, WHITE);
            }
        }
    },
    object.renderable);
}

void RenderSystem::PushBlendMode(RenderContext& ctx, const SceneLayerBlend& blend) {
    switch (blend) {
        case SceneLayerBlend::Normal:
            ctx.PushBlendMode(BLEND_ALPHA);
            break;
        case SceneLayerBlend::Additive:
            ctx.PushBlendMode(BLEND_ADDITIVE);
            break;
        case SceneLayerBlend::Multiply:
            ctx.PushBlendMode(BLEND_MULTIPLIED);
            break;
        case SceneLayerBlend::Alpha:
            ctx.PushBlendMode(BLEND_ALPHA);
            break;
        default:
            ctx.PushBlendMode(BLEND_ALPHA);
            break;
    }
}

Matrix RenderSystem::CalculateTransform(Entity cameraEntity, const SceneLayer& layer) {
    auto& cameraPosition = world->GetComponent<PositionComponent>(cameraEntity);
    auto& camera = world->GetComponent<CameraComponent>(cameraEntity);

    switch (layer.space) {
        case SceneLayerSpace::Screen2D: {
            return MatrixIdentity();
        }
        case SceneLayerSpace::World2D: {
            Vector2 viewportCenter = {
                camera.viewport.width * 0.5f,
                camera.viewport.height * 0.5f};

            Matrix centerTranslation = MatrixTranslate(viewportCenter.x, viewportCenter.y, 0);
            Matrix scale = MatrixScale(camera.zoom, camera.zoom, 1.0f);
            Matrix cameraTranslation = MatrixTranslate(-cameraPosition.x, -cameraPosition.y, 0);

            return MatrixMultiply(MatrixMultiply(centerTranslation, scale), cameraTranslation);
        }
    }

    return MatrixIdentity();
}

std::vector<Renderable> RenderSystem::GetRenderables(Entity entity) {
    std::vector<Renderable> renderables;

    if (world->HasComponent<RectangleComponent>(entity)) {
        renderables.push_back(world->GetComponent<RectangleComponent>(entity));
    }
    if (world->HasComponent<CircleComponent>(entity)) {
        renderables.push_back(world->GetComponent<CircleComponent>(entity));
    }
    if (world->HasComponent<TextComponent>(entity)) {
        renderables.push_back(world->GetComponent<TextComponent>(entity));
    }
    if (world->HasComponent<SpriteComponent>(entity)) {
        renderables.push_back(world->GetComponent<SpriteComponent>(entity));
    }
    if (world->HasComponent<LightComponent>(entity)) {
        renderables.push_back(world->GetComponent<LightComponent>(entity));
    }
    if (world->HasComponent<DebugComponent>(entity)) {
        renderables.push_back(world->GetComponent<DebugComponent>(entity));
    }

    return renderables;
}

std::string RenderSystem::GetLayerName(Entity entity) {
    if (world->HasComponent<LayerComponent>(entity)) {
        return world->GetComponent<LayerComponent>(entity).name;
    }
    return "default";
}

}  // namespace Elysium::Systems

REGISTER_SYSTEM(Elysium::Systems::RenderSystem)