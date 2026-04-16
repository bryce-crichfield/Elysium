#include "Systems/RenderSystem.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>
#include "Components/BoundsComponent.h"
#include "Components/CameraComponent.h"
#include "Components/CircleComponent.h"
#include "Components/HealthComponent.h"
#include "Components/LayerComponent.h"
#include "Components/LightComponent.h"
#include "Components/NameComponent.h"
#include "Components/PositionComponent.h"
#include "Components/RectangleComponent.h"
#include "Components/ScaleComponent.h"
#include "Components/SelectionComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/TextComponent.h"
#include "Components/TileComponent.h"
#include "Core/Tile.h"
#include "Core/Application.h"
#include "Core/Common.h"
#include "Core/Entity.h"
#include "Core/Path.h"
#include "Core/RenderContext.h"
#include "Core/Scene.h"
#include "Core/SystemRegistry.h"
#include "Services/AssetService.h"
#include "Services/SceneService.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

namespace Elysium::Systems {

static RenderSystem* s_currentRenderSystem = nullptr;

RenderSystem::RenderSystem(Context context) : System(context) {
    _previousRenderSystem = s_currentRenderSystem;
    s_currentRenderSystem = this;
    _renderQueue.reserve(4096);
}

RenderSystem::~RenderSystem() {
    if (s_currentRenderSystem == this)
        s_currentRenderSystem = _previousRenderSystem;
    if (_lightMap.id != 0) {
        UnloadRenderTexture(_lightMap);
    }
}

RenderSystem* RenderSystem::GetCurrent() {
    return s_currentRenderSystem;
}

void RenderSystem::IssueDrawCommand(DrawCommand cmd) {
    _drawCommands.push_back(std::move(cmd));
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
    ProfileN("RenderSystem Draw");

    // Cache isometric flag once — all tiles share the same isometric state.
    if (!_isIsometricCached) {
        _isIsometricCached = true;
        world->Query<TileComponent>([&](Entity, const TileComponent& t) {
            _isIsometric = t.isIsometric;
        });
    }

    FindCameras();
    RenderContext ctx;

    for (auto& cameraEntity : _cameraEntities) {
        RenderView(ctx, cameraEntity);
    }

    _drawCommands.clear();
}

void RenderSystem::FindCameras() {
    ProfileN("RenderSystem FindCameras");
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
    ProfileN("RenderSystem RenderView");

    auto& camera = world->GetComponent<CameraComponent>(cameraEntity);
    if (!camera.isVisible)
        return;

    ctx.PushScissorMode(
        (int)camera.viewport.x,
        (int)camera.viewport.y,
        (int)camera.viewport.width,
        (int)camera.viewport.height);

    // Build layer name -> index map once per frame.
    // This avoids per-entity string hashing during collect.
    std::vector<SceneLayer> layers = scene->GetLayers();
    std::unordered_map<std::string, uint8_t> layerNameToIndex;
    layerNameToIndex.reserve(layers.size());
    for (size_t i = 0; i < layers.size(); i++) {
        layerNameToIndex[layers[i].name] = (uint8_t)i;
    }

    // Default layer index — resolved once, not per entity
    uint8_t defaultLayerIndex = 0;
    auto defaultIt = layerNameToIndex.find("default");
    if (defaultIt != layerNameToIndex.end()) {
        defaultLayerIndex = defaultIt->second;
    }

    _renderQueue.clear();

    Vector2 cameraPos = {0, 0};
    if (world->HasComponent<PositionComponent>(cameraEntity)) {
        auto& pos = world->GetComponent<PositionComponent>(cameraEntity);
        cameraPos = { pos.x, pos.y };
    }

    float halfW = (camera.viewport.width  * 0.5f) / camera.zoom;
    float halfH = (camera.viewport.height * 0.5f) / camera.zoom;

    float worldLeft   = cameraPos.x - halfW;
    worldLeft -= 100;
    float worldRight  = cameraPos.x + halfW;
    worldRight += 100;
    float worldTop    = cameraPos.y - halfH;
    worldTop -= 100;
    float worldBottom = cameraPos.y + halfH;
    worldBottom += 100;

    {
        ProfileN("Collect Renderables");
        world->Query<PositionComponent>([&](Entity entity, auto& pos) {
            if (world->HasComponent<CameraComponent>(entity))
                return;


            uint8_t mask = 0;
            if (world->HasComponent<LightComponent>(entity))     mask |= RC_Light;

            auto hasLight = (mask & RC_Light) != 0;
            if (!hasLight && (pos.x < worldLeft || pos.x > worldRight ||
                pos.y < worldTop  || pos.y > worldBottom))
                return;
                
            if (world->HasComponent<RectangleComponent>(entity)) mask |= RC_Rectangle;
            if (world->HasComponent<CircleComponent>(entity))    mask |= RC_Circle;
            if (world->HasComponent<TextComponent>(entity))      mask |= RC_Text;
            if (world->HasComponent<SpriteComponent>(entity))    mask |= RC_Sprite;
            if (world->HasComponent<TileComponent>(entity)) {
                const auto& tc = world->GetComponent<TileComponent>(entity);
                if (!tc.tileName.empty()) mask |= RC_Tile;
            }
            if (!mask) return;

            uint8_t layerIndex   = defaultLayerIndex;
            uint8_t isWorldSpace = 0;
            if (world->HasComponent<LayerComponent>(entity)) {
                const auto& layerComp = world->GetComponent<LayerComponent>(entity);
                auto it = layerNameToIndex.find(layerComp.name);
                if (it != layerNameToIndex.end()) {
                    layerIndex   = it->second;
                    isWorldSpace = (layers[layerIndex].space == SceneLayerSpace::World2D) ? 1 : 0;
                }
            }

            // hierarchyDepth not yet populated here — kept as 0 for now,
            // same as the previous path which never set it during collect either.
            _renderQueue.push_back({ layerIndex, 0, mask, isWorldSpace, pos.y, pos.x, entity });
        });
    }

    {
        ProfileN("Sort Renderables");
        std::sort(_renderQueue.begin(), _renderQueue.end(),
            [](const RenderKey& a, const RenderKey& b) {
                if (a.layerIndex != b.layerIndex)
                    return a.layerIndex < b.layerIndex;
                if (a.hierarchyDepth != b.hierarchyDepth)
                    return a.hierarchyDepth < b.hierarchyDepth;
                // Y-sort only for World2D layers (painter's algorithm depth ordering).
                // Screen2D layers preserve XML declaration order via entity ID.
                if (a.isWorldSpace)
                    return a.y < b.y;
                return a.entity < b.entity;
            });
    }

    // Render each visible layer, slicing the sorted queue by layerIndex
    size_t queueStart = 0;
    for (size_t li = 0; li < layers.size(); li++) {
        const SceneLayer& layer = layers[li];
        if (!layer.isVisible) {
            // Skip past any keys for this layer index
            while (queueStart < _renderQueue.size() && _renderQueue[queueStart].layerIndex == (uint8_t)li)
                queueStart++;
            continue;
        }

        // Find the contiguous slice of keys belonging to this layer
        size_t sliceStart = queueStart;
        while (queueStart < _renderQueue.size() && _renderQueue[queueStart].layerIndex == (uint8_t)li)
            queueStart++;
        size_t sliceEnd = queueStart;

        bool hasDrawCmds = std::any_of(_drawCommands.begin(), _drawCommands.end(),
            [&](const DrawCommand& cmd) {
                return std::visit([&](const auto& c) { return c.layer == layer.name; }, cmd);
            });

        if (sliceStart == sliceEnd && !hasDrawCmds)
            continue;

        std::span<const RenderKey> slice(_renderQueue.data() + sliceStart, sliceEnd - sliceStart);

        if (layer.isComposited) {
            RenderCompositedLayer(ctx, cameraEntity, layer, slice);
        } else {
            RenderImmediateLayer(ctx, cameraEntity, layer, slice);
        }
    }

    ctx.PopScissorMode();
}

void RenderSystem::RenderImmediateLayer(RenderContext& ctx, Entity cameraEntity, const SceneLayer& layer, std::span<const RenderKey> keys) {
    ProfileN("Render Immediate Layer");
    Matrix viewProjectionTransform = CalculateTransform(cameraEntity, layer);
    PushBlendMode(ctx, layer.layerBlend);
    ctx.PushMatrix();
    ctx.MultiplyMatrix(viewProjectionTransform);

    {
        ProfileN("Render Objects");
        for (const auto& key : keys) {
            RenderEntity(ctx, key, layer);
        }
    }

    {
        ProfileN("Render DrawCommands");
        RenderDrawCommands(ctx, layer);
    }

    ctx.PopMatrix();
    ctx.PopBlendMode();
}

void RenderSystem::RenderCompositedLayer(RenderContext& ctx, Entity cameraEntity, const SceneLayer& layer, std::span<const RenderKey> keys) {
    ProfileN("Render Composited Layer");

    auto& camera = world->GetComponent<CameraComponent>(cameraEntity);
    Matrix layerTransform = CalculateTransform(cameraEntity, layer);

    int w = (int)camera.viewport.width;
    int h = (int)camera.viewport.height;
    RenderTexture2D& compositionBuffer = EnsureLightMap(w, h);

    ctx.PopScissorMode();

    ctx.BeginTextureMode(compositionBuffer);
    ClearBackground(layer.ambient);

    PushBlendMode(ctx, layer.layerBlend);
    ctx.PushMatrix();
    ctx.MultiplyMatrix(layerTransform);

    for (const auto& key : keys) {
        RenderEntity(ctx, key, layer);
    }

    RenderDrawCommands(ctx, layer);

    ctx.PopMatrix();
    ctx.PopBlendMode();

    ctx.EndTextureMode();

    // Restore SceneService's framebuffer
    auto& sceneService = Application::GetInstance().GetService<Services::SceneService>();
    ctx.BeginTextureMode(sceneService.GetFramebuffer());

    PushBlendMode(ctx, layer.compositeBlend);
    Rectangle src = {0, 0, (float)w, -(float)h};
    Rectangle dst = {camera.viewport.x, camera.viewport.y, (float)w, (float)h};
    ctx.DrawTexturePro(compositionBuffer.texture, src, dst, {0, 0}, 0.0f, WHITE);

    ctx.PopBlendMode();

    ctx.PushScissorMode(
        (int)camera.viewport.x,
        (int)camera.viewport.y,
        (int)camera.viewport.width,
        (int)camera.viewport.height);
}

void RenderSystem::RenderEntity(RenderContext& ctx, const RenderKey& key, const SceneLayer& layer) {
    Vector2 pos = { key.x, key.y };

    if (key.componentMask & RC_Tile)
        RenderTile(ctx, key.entity, pos, layer);
    if (key.componentMask & RC_Rectangle)
        RenderRectangle(ctx, key.entity, pos, layer);
    if (key.componentMask & RC_Circle)
        RenderCircle(ctx, key.entity, pos, layer);
    if (key.componentMask & RC_Sprite)
        RenderSprite(ctx, key.entity, pos, layer);
    if (key.componentMask & RC_Text)
        RenderText(ctx, key.entity, pos, layer);
    if (key.componentMask & RC_Light)
        RenderLight(ctx, key.entity, pos, layer);
}

void RenderSystem::RenderRectangle(RenderContext& ctx, Entity entity, Vector2 pos, const SceneLayer& layer) {
    const auto& component = world->GetComponent<RectangleComponent>(entity);

    float topLeftX = (layer.space == SceneLayerSpace::Screen2D)
        ? pos.x : pos.x - component.width * 0.5f;
    float topLeftY = (layer.space == SceneLayerSpace::Screen2D)
        ? pos.y : pos.y - component.height * 0.5f;

    ctx.DrawRectangle(topLeftX, topLeftY, component.width, component.height, component.background);
    if (component.border.a > 0) {
        ctx.DrawRectangleLines(topLeftX, topLeftY, component.width, component.height, component.border);
    }

    if (!component.textureName.empty()) {
        auto& assets = Application::GetInstance().GetService<Elysium::Services::AssetService>();
        Texture2D texture = assets.GetTexture(Path(component.textureName));
        if (texture.id != 0) {
            Rectangle sourceRect = { 0, 0, (float)texture.width, (float)texture.height };
            Rectangle destRect   = { topLeftX, topLeftY, component.width, component.height };
            ctx.DrawTexturePro(texture, sourceRect, destRect, {0, 0}, 0.0f, WHITE);
        } else {
            assets.LoadAsset(AssetType::TEXTURE, Path(component.textureName));
        }
    }
}

void RenderSystem::RenderCircle(RenderContext& ctx, Entity entity, Vector2 pos, const SceneLayer& layer) {
    const auto& component = world->GetComponent<CircleComponent>(entity);
    ctx.DrawCircle(pos.x, pos.y, component.radius, component.background);
    if (component.border.a > 0) {
        ctx.DrawCircleLines(pos.x, pos.y, component.radius, component.border);
    }
}

void RenderSystem::RenderText(RenderContext& ctx, Entity entity, Vector2 pos, const SceneLayer& layer) {
    const auto& component = world->GetComponent<TextComponent>(entity);

    float scaleX = 1.0f, scaleY = 1.0f;
    if (world->HasComponent<ScaleComponent>(entity)) {
        const auto& scale = world->GetComponent<ScaleComponent>(entity);
        scaleX = scale.x;
        scaleY = scale.y;
    }

    int scaledFontSize = (int)(component.fontSize * ((scaleX + scaleY) * 0.5f));
    int textWidth = MeasureText(component.content.c_str(), scaledFontSize);

    float drawX, drawY;
    if (layer.space == SceneLayerSpace::Screen2D && world->HasComponent<RectangleComponent>(entity)) {
        const auto& rect = world->GetComponent<RectangleComponent>(entity);
        drawX = pos.x + (rect.width  - (float)textWidth)      * 0.5f;
        drawY = pos.y + (rect.height - (float)scaledFontSize)  * 0.5f;
    } else {
        drawX = pos.x - textWidth      * 0.5f;
        drawY = pos.y - scaledFontSize * 0.5f;
    }

    ctx.DrawText(component.content.c_str(), drawX, drawY, scaledFontSize, component.color);
}

void RenderSystem::RenderSprite(RenderContext& ctx, Entity entity, Vector2 pos, const SceneLayer& layer) {
    const auto& component = world->GetComponent<SpriteComponent>(entity);
    auto& assets = Application::GetInstance().GetService<Elysium::Services::AssetService>();

    Sprite sprite = assets.GetSprite(Path(component.spriteName));
    if (sprite.name.empty()) return;

    auto sheetIt = sprite.sheets.find(component.sheetName);
    if (sheetIt == sprite.sheets.end()) return;
    const SpriteSheet& sheet = sheetIt->second;

    auto seqIt = sheet.sequences.find(component.sequenceName);
    if (seqIt == sheet.sequences.end()) return;
    const SpriteSequence& sequence = seqIt->second;

    if (sequence.indices.empty()) return;

    size_t frameIdx   = component.sequenceIndex % sequence.indices.size();
    size_t linearIndex = sequence.indices[frameIdx];

    Texture2D texture = assets.GetTexture(Path("Sprites/" + sheet.path));
    if (texture.id == 0) return;

    float frameWidth  = (float)texture.width  / (float)sheet.cols;
    float frameHeight = (float)texture.height / (float)sheet.rows;
    size_t col = linearIndex % sheet.cols;
    size_t row = linearIndex / sheet.cols;

    Rectangle sourceRect = { col * frameWidth, row * frameHeight, frameWidth, frameHeight };

    float scaleX = 1.0f, scaleY = 1.0f;
    if (world->HasComponent<ScaleComponent>(entity)) {
        const auto& scale = world->GetComponent<ScaleComponent>(entity);
        scaleX = scale.x;
        scaleY = scale.y;
    }

    float scaledWidth  = frameWidth  * scaleX;
    float scaledHeight = frameHeight * scaleY;

    Rectangle destRect = {
        pos.x - scaledWidth  * sprite.originX,
        pos.y - scaledHeight * sprite.originY,
        scaledWidth,
        scaledHeight
    };

    ctx.DrawTexturePro(texture, sourceRect, destRect, {0, 0}, 0.0f, WHITE);
}

void RenderSystem::RenderTile(RenderContext& ctx, Entity entity, Vector2 pos, const SceneLayer& layer) {
    const auto& comp = world->GetComponent<TileComponent>(entity);
    auto& assets = Application::GetInstance().GetService<Elysium::Services::AssetService>();

    Tile tile = assets.GetTile(Path(comp.tileName));
    if (tile.IsEmpty()) return;

    auto varIt = tile.variants.find(comp.variantName);
    if (varIt == tile.variants.end()) {
        varIt = tile.variants.find("default");
        if (varIt == tile.variants.end()) return;
    }
    const TileVariant& variant = varIt->second;

    Texture2D texture = assets.GetTexture(Path("Tiles/" + tile.sheet.path));
    if (texture.id == 0) return;

    float frameWidth  = (float)texture.width  / (float)tile.sheet.cols;
    float frameHeight = (float)texture.height / (float)tile.sheet.rows;

    Rectangle sourceRect = {
        (float)variant.col * frameWidth,
        (float)variant.row * frameHeight,
        frameWidth,
        frameHeight
    };
    Rectangle destRect = {
        pos.x - frameWidth  * tile.originX,
        pos.y - frameHeight * tile.originY,
        frameWidth,
        frameHeight
    };

    ctx.DrawTexturePro(texture, sourceRect, destRect, {0, 0}, 0.0f, WHITE);
}

void RenderSystem::RenderLight(RenderContext& ctx, Entity entity, Vector2 pos, const SceneLayer& layer) {
    const auto& component = world->GetComponent<LightComponent>(entity);
    const int numRings = 8;

    for (int i = 0; i < numRings; i++) {
        float t     = (float)i / numRings;
        float power = 1.0f + component.intensity * 4.0f;
        float curve = powf(1.0f - t, power);
        float ringRadius = component.radius * curve;

        Color ringColor = component.color;
        ringColor.a = (unsigned char)(component.color.a / numRings);
        Color ringEdge  = { ringColor.r, ringColor.g, ringColor.b, 0 };

        if (_isIsometric) {
            ctx.DrawEllipseGradient(pos.x, pos.y, ringRadius, ringRadius * 0.5f, ringColor, ringEdge);
        } else {
            ctx.DrawCircleGradient(pos.x, pos.y, ringRadius, ringColor, ringEdge);
        }
    }
}

void RenderSystem::RenderDrawCommands(RenderContext& ctx, const SceneLayer& layer) {
    for (const auto& cmd : _drawCommands) {
        std::visit([&](const auto& c) {
            if (c.layer != layer.name)
                return;
            using T = std::decay_t<decltype(c)>;
            if constexpr (std::is_same_v<T, DrawCircleCmd>) {
                ctx.DrawCircleLines(c.x, c.y, c.radius, c.color);
            } else if constexpr (std::is_same_v<T, DrawLineCmd>) {
                ctx.DrawLine(c.x1, c.y1, c.x2, c.y2, c.color);
            } else if constexpr (std::is_same_v<T, DrawRectCmd>) {
                ctx.DrawRectangle(c.x, c.y, c.width, c.height, c.color);
            } else if constexpr (std::is_same_v<T, DrawEllipseCmd>) {
                ctx.DrawEllipseLines(c.x, c.y, c.radiusH, c.radiusV, c.color);
            } else if constexpr (std::is_same_v<T, DrawTextCmd>) {
                ctx.DrawText(c.text.c_str(), c.x, c.y, c.fontSize, c.color);
            } else if constexpr (std::is_same_v<T, DrawPolygonCmd>) {
                const auto& pts = c.points;
                for (int i = 1; i + 1 < (int)pts.size(); i++) {
                    DrawTriangle(pts[0], pts[i + 1], pts[i], c.color);
                }
            }
        }, cmd);
    }
}

void RenderSystem::PushBlendMode(RenderContext& ctx, const SceneLayerBlend& blend) {
    switch (blend) {
        case SceneLayerBlend::Normal:   ctx.PushBlendMode(BLEND_ALPHA);      break;
        case SceneLayerBlend::Additive: ctx.PushBlendMode(BLEND_ADDITIVE);   break;
        case SceneLayerBlend::Multiply: ctx.PushBlendMode(BLEND_MULTIPLIED); break;
        case SceneLayerBlend::Alpha:    ctx.PushBlendMode(BLEND_ALPHA);      break;
        default:                        ctx.PushBlendMode(BLEND_ALPHA);      break;
    }
}

Matrix RenderSystem::CalculateTransform(Entity cameraEntity, const SceneLayer& layer) {
    auto& cameraPosition = world->GetComponent<PositionComponent>(cameraEntity);
    auto& camera         = world->GetComponent<CameraComponent>(cameraEntity);

    switch (layer.space) {
        case SceneLayerSpace::Screen2D:
            return MatrixIdentity();
        case SceneLayerSpace::World2D: {
            Vector2 viewportCenter = {
                camera.viewport.width  * 0.5f,
                camera.viewport.height * 0.5f
            };
            Matrix centerTranslation = MatrixTranslate(viewportCenter.x, viewportCenter.y, 0);
            Matrix scale             = MatrixScale(camera.zoom, camera.zoom, 1.0f);
            Matrix cameraTranslation = MatrixTranslate(-cameraPosition.x, -cameraPosition.y, 0);
            return MatrixMultiply(MatrixMultiply(centerTranslation, scale), cameraTranslation);
        }
    }

    return MatrixIdentity();
}

std::string RenderSystem::GetLayerName(Entity entity) {
    if (world->HasComponent<LayerComponent>(entity)) {
        return world->GetComponent<LayerComponent>(entity).name;
    }
    return "default";
}

}  // namespace Elysium::Systems

REGISTER_SYSTEM(Elysium::Systems::RenderSystem)