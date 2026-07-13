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
#include "Components/TransformComponent.h"
#include "Components/RectangleComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/TextComponent.h"
#include "Components/TileComponent.h"
#include "Components/ParentComponent.h"
#include "Core/Tile.h"
#include "Core/Application.h"
#include "Core/Common.h"
#include "Core/Entity.h"
#include "Core/Geometry.h"
#include "Core/Path.h"
#include "Core/RenderContext.h"
#include "Core/Scene.h"
#include "Core/SystemRegistry.h"
#include "Services/LogService.h"
#include "Services/AssetService.h"
#include "Services/EditorService.h"
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

    ComputeHiddenEntities();
    FindCameras();
    RenderContext ctx;

    if (Application::GetInstance().GetMode() == AppMode::Editor) {
        // Editor mode renders through the free editor camera rather than any in-scene
        // CameraComponent — real cameras are drawn as gizmos instead (DrawCameraGizmos).
        auto& editorService = Application::GetInstance().GetService<Services::EditorService>();
        const auto& config = Application::GetInstance().GetConfig();
        auto& editorCam = editorService.GetEditorCamera();

        CameraView view{
            editorCam.position,
            editorCam.zoom != 0.0f ? editorCam.zoom : 1.0f,
            Rectangle{0, 0, (float)config.framebufferWidth, (float)config.framebufferHeight}
        };

        RenderView(ctx, view);
        DrawAxes(ctx, view);
        DrawCameraGizmos(ctx, view);
        DrawSelectionOverlay(ctx, view);
    } else {
        for (auto& cameraEntity : _cameraEntities) {
            auto& camera = world->GetComponent<CameraComponent>(cameraEntity);
            if (!camera.isVisible) continue;
            RenderView(ctx, MakeCameraView(cameraEntity));
        }
    }

    _drawCommands.clear();
}

void RenderSystem::DrawSelectionOverlay(RenderContext& ctx, const CameraView& view) {
    auto& editorService = Application::GetInstance().GetService<Services::EditorService>();
    const auto& selected = editorService.GetSelectedEntities();
    if (selected.empty()) return;

    const Color highlightColor = { 255, 200, 0, 255 };
    const float defaultHalfSize = 16.0f;

    ctx.PushScissorMode(
        (int)view.viewport.x, (int)view.viewport.y,
        (int)view.viewport.width, (int)view.viewport.height);

    // Assumes World2D space — Screen2D-layer selections won't line up correctly yet.
    // Pre-divide screen-constant sizes (line thickness) by zoom before drawing inside the
    // zoom-scaled matrix below, so they net out to a constant pixel size on screen instead
    // of growing/shrinking with the camera's zoom.
    float invZoom = 1.0f / (view.zoom != 0.0f ? view.zoom : 1.0f);

    ctx.PushMatrix();
    ctx.MultiplyMatrix(CalculateTransform(view, SceneLayer{}));

    for (Entity entity : selected) {
        if (!world->HasComponent<TransformComponent>(entity)) continue;
        const auto& t = world->GetComponent<TransformComponent>(entity);
        Vector2 pos = { t.worldX, t.worldY };

        Rectangle outline;
        if (world->HasComponent<RectangleComponent>(entity)) {
            const auto& rect = world->GetComponent<RectangleComponent>(entity);
            float halfW = rect.width * 0.5f, halfH = rect.height * 0.5f;
            outline = { pos.x - halfW, pos.y - halfH, halfW * 2.0f, halfH * 2.0f };
        } else if (world->HasComponent<CircleComponent>(entity)) {
            const auto& circle = world->GetComponent<CircleComponent>(entity);
            outline = { pos.x - circle.radius, pos.y - circle.radius, circle.radius * 2.0f, circle.radius * 2.0f };
        } else if (world->HasComponent<TextureComponent>(entity) && !world->GetComponent<TextureComponent>(entity).textureName.empty()) {
            // Mirrors RenderSprite's dest-rect math (scale + origin, ignoring rotation —
            // the existing Rectangle/Circle outlines above are axis-aligned too) so the
            // highlight actually wraps the drawn sprite instead of a fixed-size box.
            const auto& tex = world->GetComponent<TextureComponent>(entity);
            float absWidth = std::fabs(tex.sourceRect.width * t.worldScaleX);
            float absHeight = std::fabs(tex.sourceRect.height * t.worldScaleY);
            outline = { pos.x - absWidth * tex.originX, pos.y - absHeight * tex.originY, absWidth, absHeight };
        } else {
            outline = { pos.x - defaultHalfSize, pos.y - defaultHalfSize, defaultHalfSize * 2.0f, defaultHalfSize * 2.0f };
        }

        ctx.DrawRectangleLinesEx(outline, 2.0f * invZoom, highlightColor);
    }

    ctx.PopMatrix();

    // Move handle — drawn in framebuffer/screen space rather than inside the zoom-scaled
    // world matrix above, so it stays a constant on-screen size regardless of camera zoom.
    // This also keeps it consistent with ViewportEditor's screen-space hit-test radius,
    // which compares against the same kMoveHandleRadius in framebuffer coordinates.
    if (selected.size() == 1 && world->HasComponent<TransformComponent>(selected[0])) {
        const auto& t = world->GetComponent<TransformComponent>(selected[0]);
        Vector2 handlePos = WorldToFramebuffer({ t.worldX, t.worldY }, view);
        const Color handleFill    = { 0, 220, 255, 255 };
        const Color handleOutline = { 15, 15, 15, 255 };

        ctx.DrawCircle(handlePos.x, handlePos.y, kMoveHandleRadius, handleFill);
        ctx.DrawCircleLines(handlePos.x, handlePos.y, kMoveHandleRadius, handleOutline);

        float crossHalf = kMoveHandleRadius * 0.5f;
        ctx.DrawLineEx(handlePos.x - crossHalf, handlePos.y, handlePos.x + crossHalf, handlePos.y, 1.5f, WHITE);
        ctx.DrawLineEx(handlePos.x, handlePos.y - crossHalf, handlePos.x, handlePos.y + crossHalf, 1.5f, WHITE);
    }

    ctx.PopScissorMode();
}

void RenderSystem::DrawAxes(RenderContext& ctx, const CameraView& editorView) {
    const Color xAxisColor = Color{ 255, 0, 0, 255 };
    const Color yAxisColor = Color{ 0, 255, 0, 255 };

    ctx.PushScissorMode(
        (int)editorView.viewport.x, (int)editorView.viewport.y,
        (int)editorView.viewport.width, (int)editorView.viewport.height);

    float invZoom = 1.0f / (editorView.zoom != 0.0f ? editorView.zoom : 1.0f);

    ctx.PushMatrix();
    ctx.MultiplyMatrix(CalculateTransform(editorView, SceneLayer{}));

    const int minVal = std::numeric_limits<int>::min() / 2;
    const int maxVal = std::numeric_limits<int>::max() / 2;

    const float lineThickness = 1.5f * invZoom;

    ctx.DrawLineEx((float)minVal, 0.0f, (float)maxVal, 0.0f, lineThickness, xAxisColor);
    ctx.DrawLineEx(0.0f, (float)minVal, 0.0f, (float)maxVal, lineThickness, yAxisColor);

    ctx.PopMatrix();
}

void RenderSystem::DrawCameraGizmos(RenderContext& ctx, const CameraView& editorView) {
    const Color cameraGizmoColor = { 255, 60, 60, 255 };
    float invZoom = 1.0f / (editorView.zoom != 0.0f ? editorView.zoom : 1.0f);

    ctx.PushMatrix();
    ctx.MultiplyMatrix(CalculateTransform(editorView, SceneLayer{}));

    for (Entity cameraEntity : _cameraEntities) {
        if (!world->HasComponent<TransformComponent>(cameraEntity)) continue;
        auto& camera = world->GetComponent<CameraComponent>(cameraEntity);
        auto& t = world->GetComponent<TransformComponent>(cameraEntity);

        float zoom = camera.zoom != 0.0f ? camera.zoom : 1.0f;
        float halfW = (camera.viewport.width  * 0.5f) / zoom;
        float halfH = (camera.viewport.height * 0.5f) / zoom;
        Rectangle rect = { t.worldX - halfW, t.worldY - halfH, halfW * 2.0f, halfH * 2.0f };

        ctx.DrawRectangleLinesEx(rect, 2.0f * invZoom, cameraGizmoColor);
    }

    ctx.PopMatrix();
}

CameraView RenderSystem::MakeCameraView(Entity cameraEntity) {
    auto& camera = world->GetComponent<CameraComponent>(cameraEntity);
    Vector2 position = {0, 0};
    if (world->HasComponent<TransformComponent>(cameraEntity)) {
        auto& transform = world->GetComponent<TransformComponent>(cameraEntity);
        position = { transform.worldX, transform.worldY };
    }
    return CameraView{ position, camera.zoom != 0.0f ? camera.zoom : 1.0f, camera.viewport };
}

void RenderSystem::ComputeHiddenEntities() {
    ProfileN("RenderSystem ComputeHiddenEntities");
    _hiddenEntities.clear();

    // Seed with entities explicitly hidden via their own LayerComponent (e.g.
    // VisibilitySystem's per-entity fog-of-war flag), then cascade that hidden
    // state down through the hierarchy so children — sprites, health bars, etc.
    // that carry no LayerComponent of their own — inherit their ancestor's
    // hidden state instead of defaulting back to visible.
    std::vector<Entity> stack;
    world->Query<LayerComponent>([&](Entity entity, const LayerComponent& layerComp) {
        if (!layerComp.isVisible) stack.push_back(entity);
    });

    const auto& allChildren = world->GetAllChildren();
    while (!stack.empty()) {
        Entity entity = stack.back();
        stack.pop_back();
        if (!_hiddenEntities.insert(entity).second) continue;  // already visited

        auto it = allChildren.find(entity);
        if (it != allChildren.end()) {
            for (Entity child : it->second) stack.push_back(child);
        }
    }
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

void RenderSystem::RenderView(RenderContext& ctx, const CameraView& view) {
    ProfileN("RenderSystem RenderView");

    ctx.PushScissorMode(
        (int)view.viewport.x,
        (int)view.viewport.y,
        (int)view.viewport.width,
        (int)view.viewport.height);

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

    Vector2 cameraPos = view.position;

    float halfW = (view.viewport.width  * 0.5f) / view.zoom;
    float halfH = (view.viewport.height * 0.5f) / view.zoom;

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
        world->Query<TransformComponent>([&](Entity entity, auto& transform) {
            if (world->HasComponent<CameraComponent>(entity))
                return;

            Vector2 pos = { transform.worldX, transform.worldY };

            uint16_t mask = 0;
            if (world->HasComponent<LightComponent>(entity))     mask |= RC_Light;

            auto hasLight = (mask & RC_Light) != 0;
            if (!hasLight && (pos.x < worldLeft || pos.x > worldRight ||
                pos.y < worldTop  || pos.y > worldBottom))
                return;

            if (world->HasComponent<RectangleComponent>(entity)) mask |= RC_Rectangle;
            if (world->HasComponent<CircleComponent>(entity))    mask |= RC_Circle;
            if (world->HasComponent<TextComponent>(entity))      mask |= RC_Text;
            if (world->HasComponent<TextureComponent>(entity))   mask |= RC_Sprite;
            if (world->HasComponent<EllipseComponent>(entity))   mask |= RC_Ellipse;
            if (world->HasComponent<LineComponent>(entity))      mask |= RC_Line;
            if (world->HasComponent<PolygonComponent>(entity))   mask |= RC_Polygon;
            if (world->HasComponent<TileComponent>(entity)) {
                const auto& tc = world->GetComponent<TileComponent>(entity);
                if (!tc.tileName.empty()) mask |= RC_Tile;
            }
            if (!mask) return;

            uint8_t layerIndex   = defaultLayerIndex;
            uint8_t isWorldSpace = 0;
            bool isVisible = true;
            if (world->HasComponent<LayerComponent>(entity)) {
                const auto& layerComp = world->GetComponent<LayerComponent>(entity);
                auto it = layerNameToIndex.find(layerComp.name);
                if (it != layerNameToIndex.end()) {
                    layerIndex   = it->second;
                    isWorldSpace = (layers[layerIndex].space == SceneLayerSpace::World2D) ? 1 : 0;
                    isVisible = layers[layerIndex].isVisible;
                }
            }

            if (!isVisible) return;
            if (_hiddenEntities.contains(entity)) return;

            // Cached by TransformSystem alongside worldX/Y each frame — no extra
            // hierarchy walk needed here, just clamp to the key's uint8_t width.
            uint8_t hierarchyDepth = (uint8_t)std::min<uint32_t>(transform.worldDepth, 255u);
            _renderQueue.push_back({ layerIndex, hierarchyDepth, 0, mask, isWorldSpace, pos.y, pos.x, entity });

            if (world->HasComponent<ParentComponent>(entity)) {
                const auto& pc = world->GetComponent<ParentComponent>(entity);
                _renderQueue.back().childIndex = (uint8_t)std::min<uint32_t>(pc.childIndex, 255u);
            } else {
                _renderQueue.back().childIndex = 0;
            }
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
                if (a.childIndex != b.childIndex)
                    return a.childIndex < b.childIndex;
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
            RenderCompositedLayer(ctx, view, layer, slice);
        } else {
            RenderImmediateLayer(ctx, view, layer, slice);
        }
    }

    ctx.PopScissorMode();
}

void RenderSystem::RenderImmediateLayer(RenderContext& ctx, const CameraView& view, const SceneLayer& layer, std::span<const RenderKey> keys) {
    ProfileN("Render Immediate Layer");

    if (layer.opacity < 1.0f) {
        // A faded layer has to be alpha-blended as a single unit, which needs a
        // render texture even though this layer isn't flagged isComposited.
        RenderCompositedLayer(ctx, view, layer, keys);
        return;
    }

    Matrix viewProjectionTransform = CalculateTransform(view, layer);
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

void RenderSystem::RenderCompositedLayer(RenderContext& ctx, const CameraView& view, const SceneLayer& layer, std::span<const RenderKey> keys) {
    ProfileN("Render Composited Layer");

    Matrix layerTransform = CalculateTransform(view, layer);

    int w = (int)view.viewport.width;
    int h = (int)view.viewport.height;
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
    Rectangle dst = {view.viewport.x, view.viewport.y, (float)w, (float)h};
    Color compositeTint = WHITE;
    compositeTint.a = (unsigned char)(255.0f * std::clamp(layer.opacity, 0.0f, 1.0f));
    ctx.DrawTexturePro(compositionBuffer.texture, src, dst, {0, 0}, 0.0f, compositeTint);

    ctx.PopBlendMode();

    ctx.PushScissorMode(
        (int)view.viewport.x,
        (int)view.viewport.y,
        (int)view.viewport.width,
        (int)view.viewport.height);
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
    if (key.componentMask & RC_Ellipse)
        RenderEllipse(ctx, key.entity, pos, layer);
    if (key.componentMask & RC_Line)
        RenderLine(ctx, key.entity, pos, layer);
    if (key.componentMask & RC_Polygon)
        RenderPolygon(ctx, key.entity, pos, layer);
}

void RenderSystem::RenderRectangle(RenderContext& ctx, Entity entity, Vector2 pos, const SceneLayer& layer) {
    const auto& component = world->GetComponent<RectangleComponent>(entity);

    float topLeftX = (layer.space == SceneLayerSpace::Screen2D)
        ? pos.x : pos.x - component.width * 0.5f;
    float topLeftY = (layer.space == SceneLayerSpace::Screen2D)
        ? pos.y : pos.y - component.height * 0.5f;

    Rectangle rect = { topLeftX, topLeftY, component.width, component.height };
    const int roundedSegments = 8;

    if (component.cornerRadius > 0.0f) {
        ctx.DrawRectangleRounded(rect, component.cornerRadius, roundedSegments, component.background);
        if (component.border.a > 0) {
            ctx.DrawRectangleRoundedLinesEx(rect, component.cornerRadius, roundedSegments, component.strokeWidth, component.border);
        }
    } else {
        ctx.DrawRectangle(topLeftX, topLeftY, component.width, component.height, component.background);
        if (component.border.a > 0) {
            ctx.DrawRectangleLinesEx(rect, component.strokeWidth, component.border);
        }
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

void RenderSystem::RenderEllipse(RenderContext& ctx, Entity entity, Vector2 pos, const SceneLayer& layer) {
    const auto& component = world->GetComponent<EllipseComponent>(entity);
    ctx.DrawEllipse(pos.x, pos.y, component.radiusH, component.radiusV, component.background);
    if (component.border.a > 0) {
        ctx.DrawEllipseLines(pos.x, pos.y, component.radiusH, component.radiusV, component.border);
    }
}

void RenderSystem::RenderLine(RenderContext& ctx, Entity entity, Vector2 pos, const SceneLayer& layer) {
    const auto& component = world->GetComponent<LineComponent>(entity);
    ctx.DrawLineEx(pos.x + component.x1, pos.y + component.y1,
                   pos.x + component.x2, pos.y + component.y2,
                   component.thickness, component.color);
}

void RenderSystem::RenderPolygon(RenderContext& ctx, Entity entity, Vector2 pos, const SceneLayer& layer) {
    const auto& component = world->GetComponent<PolygonComponent>(entity);
    if (component.points.size() < 3) return;

    std::vector<Vector2> worldPoints;
    worldPoints.reserve(component.points.size());
    for (const auto& p : component.points) {
        worldPoints.push_back({ pos.x + p.x, pos.y + p.y });
    }

    if (component.fill.a > 0) {
        std::vector<Vector2> triangles = TriangulatePolygon(worldPoints);
        ctx.DrawTriangleList(triangles, component.fill);
    }
    if (component.border.a > 0) {
        for (size_t i = 0; i < worldPoints.size(); i++) {
            const Vector2& a = worldPoints[i];
            const Vector2& b = worldPoints[(i + 1) % worldPoints.size()];
            ctx.DrawLine(a.x, a.y, b.x, b.y, component.border);
        }
    }
}

void RenderSystem::RenderText(RenderContext& ctx, Entity entity, Vector2 pos, const SceneLayer& layer) {
    const auto& component = world->GetComponent<TextComponent>(entity);

    float scaleX = 1.0f, scaleY = 1.0f;
    if (world->HasComponent<TransformComponent>(entity)) {
        const auto& transform = world->GetComponent<TransformComponent>(entity);
        scaleX = transform.worldScaleX;
        scaleY = transform.worldScaleY;
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
    // Resolved once per frame-change by SpriteSystem; nothing left to look up here
    // beyond fetching the actual texture handle for the draw call.
    if (!world->HasComponent<TextureComponent>(entity)) return;
    const auto& tex = world->GetComponent<TextureComponent>(entity);
    if (tex.textureName.empty()) return;

    auto& assets = Application::GetInstance().GetService<Elysium::Services::AssetService>();
    Texture2D texture = assets.GetTexture(Path(tex.textureName));
    if (texture.id == 0) return;

    float scaleX = 1.0f, scaleY = 1.0f, rotation = 0.0f;
    if (world->HasComponent<TransformComponent>(entity)) {
        const auto& transform = world->GetComponent<TransformComponent>(entity);
        scaleX = transform.worldScaleX;
        scaleY = transform.worldScaleY;
        rotation = transform.worldRotation;
    }

    float scaledWidth  = tex.sourceRect.width  * scaleX;
    float scaledHeight = tex.sourceRect.height * scaleY;

    // DrawTexturePro ignores the sign of destRect's width/height (it just takes
    // abs()), so a negative world scale (e.g. a mirrored TransformComponent, as
    // produced by SVG-imported assets whose <use> transforms reflect a shape)
    // can't be expressed through destRect at all. Instead: horizontal mirroring
    // is expressed via a negated source-rect width (which DrawTexturePro does
    // honor), and vertical-only mirroring via flipHorizontal + an extra 180
    // degree rotation, since flipVertical == rotate180(flipHorizontal).
    bool flipHorizontal = scaledWidth  < 0.0f;
    bool flipVertical   = scaledHeight < 0.0f;

    Rectangle sourceRect = tex.sourceRect;
    if (flipHorizontal != flipVertical) sourceRect.width = -sourceRect.width;
    float drawRotation = rotation + (flipVertical ? 180.0f : 0.0f);

    float absWidth  = std::fabs(scaledWidth);
    float absHeight = std::fabs(scaledHeight);

    // origin passed to DrawTexturePro is in destRect-local space (i.e. already
    // scaled), since that's the space the rotation is applied around.
    Vector2 origin = { absWidth * tex.originX, absHeight * tex.originY };

    Rectangle destRect = {
        pos.x,
        pos.y,
        absWidth,
        absHeight
    };

    SetTextureFilter(texture, tex.filterMode == TextureFilterMode::Bilinear ? TEXTURE_FILTER_BILINEAR : TEXTURE_FILTER_POINT);
    ctx.DrawTexturePro(texture, sourceRect, destRect, origin, drawRotation, tex.tint);
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

    ctx.DrawTexturePro(texture, sourceRect, destRect, {0, 0}, 0.0f, comp.tint);
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
                std::vector<Vector2> triangles = TriangulatePolygon(c.points);
                ctx.DrawTriangleList(triangles, c.color);
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

std::vector<Entity> RenderSystem::Pick(Vector2 fbPos, Entity cameraEntity) {
    if (!world->HasComponent<CameraComponent>(cameraEntity) ||
        !world->HasComponent<TransformComponent>(cameraEntity)) {
        return {};
    }
    return Pick(fbPos, MakeCameraView(cameraEntity));
}

std::vector<Entity> RenderSystem::Pick(Vector2 fbPos, const CameraView& view) {
    std::vector<Entity> hits;

    // Inverse of CalculateTransform's World2D branch — same viewport-center/zoom/camera
    // math as RenderView, run backwards to turn a framebuffer point into world space.
    Vector2 viewportCenter = { view.viewport.width * 0.5f, view.viewport.height * 0.5f };
    float zoom = view.zoom != 0.0f ? view.zoom : 1.0f;
    Vector2 worldPos = {
        (fbPos.x - viewportCenter.x) / zoom + view.position.x,
        (fbPos.y - viewportCenter.y) / zoom + view.position.y
    };

    // _renderQueue is sorted in paint order (layer, then depth/y within layer).
    // Walking it back-to-front yields topmost-drawn-first, matching what's on screen.
    for (auto it = _renderQueue.rbegin(); it != _renderQueue.rend(); ++it) {
        const RenderKey& key = *it;
        Vector2 testPos = key.isWorldSpace ? worldPos : fbPos;
        Vector2 pos = { key.x, key.y };

        bool hit = false;
        if (!hit && (key.componentMask & RC_Rectangle)) hit = PickRectangle(key.entity, testPos, pos, key.isWorldSpace);
        if (!hit && (key.componentMask & RC_Circle))    hit = PickCircle(key.entity, testPos, pos);
        if (!hit && (key.componentMask & RC_Ellipse))   hit = PickEllipse(key.entity, testPos, pos);
        if (!hit && (key.componentMask & RC_Sprite))    hit = PickSprite(key.entity, testPos, pos);
        if (!hit && (key.componentMask & RC_Text))      hit = PickText(key.entity, testPos, pos, key.isWorldSpace);
        if (!hit && (key.componentMask & RC_Light))     hit = PickLight(key.entity, testPos, pos);
        if (!hit && (key.componentMask & RC_Polygon))   hit = PickPolygon(key.entity, testPos, pos);
        // RC_Tile and RC_Line are not yet supported — isometric tile picking needs grid-aware
        // math, and thin lines are rarely useful to click. Follow-up if the need comes up.

        if (hit) hits.push_back(key.entity);
    }

    return hits;
}

Vector2 RenderSystem::WorldToFramebuffer(Vector2 worldPos, Entity cameraEntity) {
    if (!world->HasComponent<CameraComponent>(cameraEntity) ||
        !world->HasComponent<TransformComponent>(cameraEntity)) {
        return worldPos;
    }
    return WorldToFramebuffer(worldPos, MakeCameraView(cameraEntity));
}

Vector2 RenderSystem::WorldToFramebuffer(Vector2 worldPos, const CameraView& view) {
    // Forward of Pick()'s inverse math above — same viewport-center/zoom/camera transform.
    Vector2 viewportCenter = { view.viewport.width * 0.5f, view.viewport.height * 0.5f };
    float zoom = view.zoom != 0.0f ? view.zoom : 1.0f;
    return {
        (worldPos.x - view.position.x) * zoom + viewportCenter.x,
        (worldPos.y - view.position.y) * zoom + viewportCenter.y
    };
}

bool RenderSystem::PickRectangle(Entity entity, Vector2 testPos, Vector2 pos, bool isWorldSpace) {
    const auto& component = world->GetComponent<RectangleComponent>(entity);
    float left = isWorldSpace ? pos.x - component.width  * 0.5f : pos.x;
    float top  = isWorldSpace ? pos.y - component.height * 0.5f : pos.y;
    return testPos.x >= left && testPos.x <= left + component.width &&
           testPos.y >= top  && testPos.y <= top + component.height;
}

bool RenderSystem::PickCircle(Entity entity, Vector2 testPos, Vector2 pos) {
    const auto& component = world->GetComponent<CircleComponent>(entity);
    return Vector2Distance(testPos, pos) <= component.radius;
}

bool RenderSystem::PickEllipse(Entity entity, Vector2 testPos, Vector2 pos) {
    const auto& component = world->GetComponent<EllipseComponent>(entity);
    if (component.radiusH <= 0.0f || component.radiusV <= 0.0f) return false;
    float dx = (testPos.x - pos.x) / component.radiusH;
    float dy = (testPos.y - pos.y) / component.radiusV;
    return (dx * dx + dy * dy) <= 1.0f;
}

bool RenderSystem::PickText(Entity entity, Vector2 testPos, Vector2 pos, bool isWorldSpace) {
    const auto& component = world->GetComponent<TextComponent>(entity);

    float scaleX = 1.0f, scaleY = 1.0f;
    if (world->HasComponent<TransformComponent>(entity)) {
        const auto& transform = world->GetComponent<TransformComponent>(entity);
        scaleX = transform.worldScaleX;
        scaleY = transform.worldScaleY;
    }
    int scaledFontSize = (int)(component.fontSize * ((scaleX + scaleY) * 0.5f));
    int textWidth = MeasureText(component.content.c_str(), scaledFontSize);

    float left, top;
    if (!isWorldSpace && world->HasComponent<RectangleComponent>(entity)) {
        const auto& rect = world->GetComponent<RectangleComponent>(entity);
        left = pos.x + (rect.width  - (float)textWidth)     * 0.5f;
        top  = pos.y + (rect.height - (float)scaledFontSize) * 0.5f;
    } else {
        left = pos.x - textWidth * 0.5f;
        top  = pos.y - scaledFontSize * 0.5f;
    }

    return testPos.x >= left && testPos.x <= left + textWidth &&
           testPos.y >= top  && testPos.y <= top + scaledFontSize;
}

bool RenderSystem::PickSprite(Entity entity, Vector2 testPos, Vector2 pos) {
    if (!world->HasComponent<TextureComponent>(entity)) return false;
    const auto& tex = world->GetComponent<TextureComponent>(entity);

    float scaleX = 1.0f, scaleY = 1.0f, rotation = 0.0f;
    if (world->HasComponent<TransformComponent>(entity)) {
        const auto& transform = world->GetComponent<TransformComponent>(entity);
        scaleX = transform.worldScaleX;
        scaleY = transform.worldScaleY;
        rotation = transform.worldRotation;
    }

    float scaledWidth  = tex.sourceRect.width  * scaleX;
    float scaledHeight = tex.sourceRect.height * scaleY;

    // Mirrors RenderSprite's flip/rotation handling exactly, so picking matches what's drawn.
    bool flipVertical = scaledHeight < 0.0f;
    float drawRotation = rotation + (flipVertical ? 180.0f : 0.0f);

    float absWidth  = std::fabs(scaledWidth);
    float absHeight = std::fabs(scaledHeight);
    Vector2 origin = { absWidth * tex.originX, absHeight * tex.originY };

    // DrawTexturePro rotates the dest rect around `pos` (see RenderSprite) — undo that
    // rotation to land back in the sprite's local, unrotated space before testing bounds.
    float rad = -drawRotation * DEG2RAD;
    float dx = testPos.x - pos.x;
    float dy = testPos.y - pos.y;
    float localX = dx * cosf(rad) - dy * sinf(rad) + origin.x;
    float localY = dx * sinf(rad) + dy * cosf(rad) + origin.y;

    return localX >= 0.0f && localX <= absWidth && localY >= 0.0f && localY <= absHeight;
}

bool RenderSystem::PickLight(Entity entity, Vector2 testPos, Vector2 pos) {
    const auto& component = world->GetComponent<LightComponent>(entity);
    return Vector2Distance(testPos, pos) <= component.radius;
}

bool RenderSystem::PickPolygon(Entity entity, Vector2 testPos, Vector2 pos) {
    const auto& component = world->GetComponent<PolygonComponent>(entity);
    if (component.points.size() < 3) return false;

    std::vector<Vector2> worldPoints;
    worldPoints.reserve(component.points.size());
    for (const auto& p : component.points) {
        worldPoints.push_back({ pos.x + p.x, pos.y + p.y });
    }
    return PointInPolygon(testPos, worldPoints);
}

Matrix RenderSystem::CalculateTransform(const CameraView& view, const SceneLayer& layer) {
    switch (layer.space) {
        case SceneLayerSpace::Screen2D:
            return MatrixIdentity();
        case SceneLayerSpace::World2D: {
            Vector2 viewportCenter = {
                view.viewport.width  * 0.5f,
                view.viewport.height * 0.5f
            };
            Matrix centerTranslation = MatrixTranslate(viewportCenter.x, viewportCenter.y, 0);
            Matrix scale             = MatrixScale(view.zoom, view.zoom, 1.0f);
            Matrix cameraTranslation = MatrixTranslate(-view.position.x, -view.position.y, 0);
            // MatrixMultiply(A, B) applies A first, then B — so this must read
            // cameraTranslation -> scale -> centerTranslation to match the scalar
            // (worldPos - position) * zoom + viewportCenter math in Pick/WorldToFramebuffer.
            // The old centerTranslation-first order was silently wrong at any zoom != 1.0
            // (it reduces to the same thing at zoom == 1.0, which is why it went unnoticed).
            return MatrixMultiply(MatrixMultiply(cameraTranslation, scale), centerTranslation);
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