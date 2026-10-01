#include "Systems/RenderSystem.h"
#include <algorithm>
#include <cmath>
#include <type_traits>
#include <unordered_set>
#include "Components/CameraComponent.h"
#include "Components/CircleComponent.h"
#include "Components/EllipseComponent.h"
#include "Components/LayerComponent.h"
#include "Components/LightComponent.h"
#include "Components/LineComponent.h"
#include "Components/MaterialComponent.h"
#include "Components/OccluderComponent.h"
#include "Components/ParentComponent.h"
#include "Components/PolygonComponent.h"
#include "Components/RectangleComponent.h"
#include "Components/ShaderComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/TextComponent.h"
#include "Components/TileComponent.h"
#include "Components/TransformComponent.h"
#include "Core/Assets/ShaderAsset.h"
#include "Core/Common.h"
#include "Core/Entity.h"
#include "Core/Geometry.h"
#include "Core/Graphics.h"
#include "Core/Log.h"
#include "Core/Path.h"
#include "Core/Framebuffer.h"
#include "Core/RenderContext.h"
#include "Core/Scene.h"
#include "Core/ServiceLocator.h"
#include "Core/Shader.h"
#include "Core/Value.h"
#include "Core/SystemRegistry.h"
#include "Core/Tile.h"
#include "Interfaces/IApplicationService.h"
#include "Interfaces/IAssetService.h"
#include "Interfaces/IEditorService.h"
#include "Interfaces/ISceneService.h"
#include "Systems/OcclusionSystem.h"
#include "Core/RaylibConvert.h"
#include "raylib.h"

namespace Elysium::Systems {

// The 3D world point lights live in. World2D positions are an isometric (2:1) picture of
// the ground; seen through an orthographic camera tilted 30 degrees down, a ground point
// (x, y) is (x, 0, 2y) in 3D, and something drawn h pixels above its ground point stands
// h / cos(30) up. Y is up, Z toward the camera. Lighting.fs does the same for each pixel.
static constexpr float kIsoCos = 0.8660254f;
static Vector3 WorldTo3D(Vector2 ground, float height) {
    return Vector3{ground.x, height / kIsoCos, ground.y * 2.0f};
}

// Ids captured at registration (see bottom of file) so CollectDrawCommands can map a
// DrawCommand variant alternative to its typeId without relying on registration order.
static RenderableTypeId g_circleCmdTypeId  = 0;
static RenderableTypeId g_lineCmdTypeId    = 0;
static RenderableTypeId g_rectCmdTypeId    = 0;
static RenderableTypeId g_ellipseCmdTypeId = 0;
static RenderableTypeId g_textCmdTypeId    = 0;
static RenderableTypeId g_polygonCmdTypeId = 0;


template <typename T>
static RenderableTypeId DrawCmdTypeId() {
    if constexpr (std::is_same_v<T, DrawCircleCmd>)       return g_circleCmdTypeId;
    else if constexpr (std::is_same_v<T, DrawLineCmd>)    return g_lineCmdTypeId;
    else if constexpr (std::is_same_v<T, DrawRectCmd>)    return g_rectCmdTypeId;
    else if constexpr (std::is_same_v<T, DrawEllipseCmd>) return g_ellipseCmdTypeId;
    else if constexpr (std::is_same_v<T, DrawTextCmd>)    return g_textCmdTypeId;
    else if constexpr (std::is_same_v<T, DrawPolygonCmd>) return g_polygonCmdTypeId;
}

// ECS-component-backed renderable types: Has/Render/Pick free functions, one per shape,
// registered at the bottom of this file. Registration order determines draw order for a
// single entity with multiple shape components (e.g. Rectangle + Text "button" pattern).

static bool HasTileImpl(const World& world, Entity entity) {
    if (!world.HasComponent<TileComponent>(entity)) return false;
    return !world.GetComponent<TileComponent>(entity).tileName.empty();
}
static bool HasRectangleImpl(const World& world, Entity entity) { return world.HasComponent<RectangleComponent>(entity); }
static bool HasCircleImpl(const World& world, Entity entity)    { return world.HasComponent<CircleComponent>(entity); }
static bool HasTextImpl(const World& world, Entity entity)      { return world.HasComponent<TextComponent>(entity); }
static bool HasEllipseImpl(const World& world, Entity entity)   { return world.HasComponent<EllipseComponent>(entity); }
static bool HasLineImpl(const World& world, Entity entity)      { return world.HasComponent<LineComponent>(entity); }
static bool HasPolygonImpl(const World& world, Entity entity)   { return world.HasComponent<PolygonComponent>(entity); }

// A bare ShaderComponent is its own renderable: it emits one record with the component's
// authored box as bounds and draws nothing itself, so RenderShadedEntity blits an empty
// (transparent) buffer through the shader and the shader generates every pixel. Only
// claims entities no other renderable does, otherwise the authored box would widen the
// union of the real shape's bounds.
static bool HasShaderImpl(const World& world, Entity entity) {
    if (!world.HasComponent<ShaderComponent>(entity)) return false;
    for (const RenderableType& type : RenderableRegistry::Instance().All()) {
        if (!type.Has || type.Has == &HasShaderImpl) continue;
        if (type.Has(world, entity)) return false;
    }
    return true;
}

static void RenderShaderImpl(RenderContext&, const RenderRecord&) {}


static std::optional<Rectangle> BoundsShaderImpl(const World& world, const RenderRecord& rec) {
    const auto& component = world.GetComponent<ShaderComponent>(rec.entity);
    float left = rec.isWorldSpace ? rec.x - component.width  * 0.5f : rec.x;
    float top  = rec.isWorldSpace ? rec.y - component.height * 0.5f : rec.y;
    return Rectangle{ left, top, component.width, component.height };
}

static bool PickShaderImpl(const World& world, const RenderRecord& rec, Vector2 testPos) {
    Rectangle b = *BoundsShaderImpl(world, rec);
    return testPos.x >= b.x && testPos.x <= b.x + b.width &&
           testPos.y >= b.y && testPos.y <= b.y + b.height;
}

static void RenderTileImpl(RenderContext& ctx, const RenderRecord& rec) {
    const auto& comp = ctx.GetWorld().GetComponent<TileComponent>(rec.entity);
    auto& assets = ctx.GetServices().Get<Elysium::Services::IAssetService>();

    auto* tileData = assets.Get<Tile>(Path(comp.tileName));
    if (!tileData) return;
    const Tile& tile = *tileData;
    if (tile.IsEmpty()) return;

    auto varIt = tile.variants.find(comp.variantName);
    if (varIt == tile.variants.end()) {
        varIt = tile.variants.find("default");
        if (varIt == tile.variants.end()) return;
    }
    const TileVariant& variant = varIt->second;

    auto* textureData = assets.Get<Texture>(Path("Tiles/" + tile.sheet.path));
    if (!textureData) return;
    const Texture& texture = *textureData;
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
        rec.x - frameWidth  * tile.originX,
        rec.y - frameHeight * tile.originY,
        frameWidth,
        frameHeight
    };

    ctx.DrawTexturePro(texture, sourceRect, destRect, {0, 0}, 0.0f, comp.tint);
}

// Axis-aligned box around a set of points (at least one).
static Rectangle BoundingBox(std::span<const Vector2> points) {
    Vector2 lo = points[0], hi = points[0];
    for (const Vector2& p : points) {
        lo = { std::min(lo.x, p.x), std::min(lo.y, p.y) };
        hi = { std::max(hi.x, p.x), std::max(hi.y, p.y) };
    }
    return { lo.x, lo.y, hi.x - lo.x, hi.y - lo.y };
}

static Rectangle Union(const Rectangle& a, const Rectangle& b) {
    const Vector2 corners[4] = { {a.x, a.y}, {a.x + a.width, a.y + a.height}, {b.x, b.y}, {b.x + b.width, b.y + b.height} };
    return BoundingBox(corners);
}

// The entity's world scale and rotation, applied about its position (rec.x, rec.y). Shapes
// describe themselves untransformed ("local": authored size, placed at the position);
// ToDraw maps a local point to where it's drawn, ToLocal maps a pick back. Negative scale
// mirrors about the position.
struct ShapeTransform {
    Vector2 pivot{};
    float scaleX = 1.0f, scaleY = 1.0f;
    float cs = 1.0f, sn = 0.0f;

    bool IsIdentity() const { return scaleX == 1.0f && scaleY == 1.0f && sn == 0.0f && cs == 1.0f; }

    Vector2 ToDraw(Vector2 p) const {
        float dx = (p.x - pivot.x) * scaleX, dy = (p.y - pivot.y) * scaleY;
        return { pivot.x + dx * cs - dy * sn, pivot.y + dx * sn + dy * cs };
    }
    Vector2 ToLocal(Vector2 p) const {
        float dx = p.x - pivot.x, dy = p.y - pivot.y;
        float rx = dx * cs + dy * sn, ry = -dx * sn + dy * cs;
        return { pivot.x + (scaleX != 0.0f ? rx / scaleX : 0.0f), pivot.y + (scaleY != 0.0f ? ry / scaleY : 0.0f) };
    }
    // Axis-aligned box around a transformed local box.
    Rectangle ToDraw(Rectangle r) const {
        if (IsIdentity()) return r;
        const Vector2 c[4] = { ToDraw({r.x, r.y}), ToDraw({r.x + r.width, r.y}),
                               ToDraw({r.x, r.y + r.height}), ToDraw({r.x + r.width, r.y + r.height}) };
        return BoundingBox(c);
    }
};

static ShapeTransform GetShapeTransform(const World& world, const RenderRecord& rec) {
    ShapeTransform xf;
    xf.pivot = { rec.x, rec.y };
    if (world.HasComponent<TransformComponent>(rec.entity)) {
        const auto& transform = world.GetComponent<TransformComponent>(rec.entity);
        xf.scaleX = transform.worldScaleX;
        xf.scaleY = transform.worldScaleY;
        if (transform.worldRotation != 0.0f) {
            float rad = transform.worldRotation * DegToRad;
            xf.cs = cosf(rad);
            xf.sn = sinf(rad);
        }
    }
    return xf;
}

// World-space rectangles hang off the entity's position by their origin; screen-space
// ones (UI) are positioned by their top-left.
static std::pair<float, float> RectangleTopLeft(const RectangleComponent& component, const RenderRecord& rec) {
    if (!rec.isWorldSpace) return { rec.x, rec.y };
    return { rec.x - component.width * component.originX, rec.y - component.height * component.originY };
}

static Rectangle LocalBoundsRectangle(const World& world, const RenderRecord& rec) {
    const auto& component = world.GetComponent<RectangleComponent>(rec.entity);
    auto [left, top] = RectangleTopLeft(component, rec);
    return Rectangle{ left, top, component.width, component.height };
}

static bool PickRectangleImpl(const World& world, const RenderRecord& rec, Vector2 testPos) {
    Vector2 p = GetShapeTransform(world, rec).ToLocal(testPos);
    Rectangle b = LocalBoundsRectangle(world, rec);
    return p.x >= b.x && p.x <= b.x + b.width && p.y >= b.y && p.y <= b.y + b.height;
}

static std::optional<Rectangle> BoundsRectangleImpl(const World& world, const RenderRecord& rec) {
    return GetShapeTransform(world, rec).ToDraw(LocalBoundsRectangle(world, rec));
}

static Rectangle LocalBoundsCircle(const World& world, const RenderRecord& rec) {
    const auto& component = world.GetComponent<CircleComponent>(rec.entity);
    return Rectangle{ rec.x - component.radius, rec.y - component.radius, component.radius * 2.0f, component.radius * 2.0f };
}

static bool PickCircleImpl(const World& world, const RenderRecord& rec, Vector2 testPos) {
    const auto& component = world.GetComponent<CircleComponent>(rec.entity);
    Vector2 p = GetShapeTransform(world, rec).ToLocal(testPos);
    return (p - Vector2{rec.x, rec.y}).Length() <= component.radius;
}

static std::optional<Rectangle> BoundsCircleImpl(const World& world, const RenderRecord& rec) {
    return GetShapeTransform(world, rec).ToDraw(LocalBoundsCircle(world, rec));
}

static Rectangle LocalBoundsEllipse(const World& world, const RenderRecord& rec) {
    const auto& component = world.GetComponent<EllipseComponent>(rec.entity);
    return Rectangle{ rec.x - component.radiusH, rec.y - component.radiusV, component.radiusH * 2.0f, component.radiusV * 2.0f };
}

static std::optional<Rectangle> BoundsEllipseImpl(const World& world, const RenderRecord& rec) {
    return GetShapeTransform(world, rec).ToDraw(LocalBoundsEllipse(world, rec));
}

static bool PickEllipseImpl(const World& world, const RenderRecord& rec, Vector2 testPos) {
    const auto& component = world.GetComponent<EllipseComponent>(rec.entity);
    if (component.radiusH <= 0.0f || component.radiusV <= 0.0f) return false;
    Vector2 p = GetShapeTransform(world, rec).ToLocal(testPos);
    float dx = (p.x - rec.x) / component.radiusH;
    float dy = (p.y - rec.y) / component.radiusV;
    return (dx * dx + dy * dy) <= 1.0f;
}

// The segment's box, grown by half its thickness on every side.
static Rectangle LocalBoundsLine(const World& world, const RenderRecord& rec) {
    const auto& component = world.GetComponent<LineComponent>(rec.entity);
    const float half = component.thickness * 0.5f;
    const Vector2 ends[2] = { {rec.x + component.x1, rec.y + component.y1}, {rec.x + component.x2, rec.y + component.y2} };
    Rectangle box = BoundingBox(ends);
    return { box.x - half, box.y - half, box.width + half * 2.0f, box.height + half * 2.0f };
}

static std::optional<Rectangle> BoundsLineImpl(const World& world, const RenderRecord& rec) {
    return GetShapeTransform(world, rec).ToDraw(LocalBoundsLine(world, rec));
}

// Within half the thickness of the segment; hairlines get a couple of units of slack.
static bool PickLineImpl(const World& world, const RenderRecord& rec, Vector2 testPos) {
    const auto& component = world.GetComponent<LineComponent>(rec.entity);
    const Vector2 p = GetShapeTransform(world, rec).ToLocal(testPos);
    const Vector2 a = { rec.x + component.x1, rec.y + component.y1 };
    const Vector2 ab = Vector2{ rec.x + component.x2, rec.y + component.y2 } - a;
    const float lengthSq = ab.x * ab.x + ab.y * ab.y;
    const float t = lengthSq > 0.0f ? std::clamp(((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / lengthSq, 0.0f, 1.0f) : 0.0f;
    const Vector2 closest = { a.x + ab.x * t, a.y + ab.y * t };
    return (p - closest).Length() <= std::max(component.thickness * 0.5f, 2.0f);
}

static bool PickPolygonImpl(const World& world, const RenderRecord& rec, Vector2 testPos) {
    const auto& component = world.GetComponent<PolygonComponent>(rec.entity);
    if (component.points.size() < 3) return false;

    std::vector<Vector2> worldPoints;
    worldPoints.reserve(component.points.size());
    for (const auto& p : component.points) {
        worldPoints.push_back({ rec.x + p.x, rec.y + p.y });
    }
    return PointInPolygon(GetShapeTransform(world, rec).ToLocal(testPos), worldPoints);
}

static std::optional<Rectangle> BoundsPolygonImpl(const World& world, const RenderRecord& rec) {
    const auto& component = world.GetComponent<PolygonComponent>(rec.entity);
    if (component.points.empty()) return std::nullopt;
    Rectangle local = BoundingBox(component.points);
    local.x += rec.x;
    local.y += rec.y;
    return GetShapeTransform(world, rec).ToDraw(local);
}

static void RenderTextImpl(RenderContext& ctx, const RenderRecord& rec) {
    const World& world = ctx.GetWorld();
    const auto& component = world.GetComponent<TextComponent>(rec.entity);

    float scaleX = 1.0f, scaleY = 1.0f;
    if (world.HasComponent<TransformComponent>(rec.entity)) {
        const auto& transform = world.GetComponent<TransformComponent>(rec.entity);
        scaleX = transform.worldScaleX;
        scaleY = transform.worldScaleY;
    }

    int scaledFontSize = (int)(component.fontSize * ((scaleX + scaleY) * 0.5f));
    int textWidth = MeasureText(component.content.c_str(), scaledFontSize);

    float drawX, drawY;
    if (!rec.isWorldSpace && world.HasComponent<RectangleComponent>(rec.entity)) {
        const auto& rect = world.GetComponent<RectangleComponent>(rec.entity);
        drawX = rec.x + (rect.width  - (float)textWidth)     * 0.5f;
        drawY = rec.y + (rect.height - (float)scaledFontSize) * 0.5f;
    } else {
        drawX = rec.x - textWidth      * 0.5f;
        drawY = rec.y - scaledFontSize * 0.5f;
    }

    ctx.DrawText(component.content.c_str(), drawX, drawY, scaledFontSize, component.color);
}

static bool PickTextImpl(const World& world, const RenderRecord& rec, Vector2 testPos) {
    const auto& component = world.GetComponent<TextComponent>(rec.entity);

    float scaleX = 1.0f, scaleY = 1.0f;
    if (world.HasComponent<TransformComponent>(rec.entity)) {
        const auto& transform = world.GetComponent<TransformComponent>(rec.entity);
        scaleX = transform.worldScaleX;
        scaleY = transform.worldScaleY;
    }
    int scaledFontSize = (int)(component.fontSize * ((scaleX + scaleY) * 0.5f));
    int textWidth = MeasureText(component.content.c_str(), scaledFontSize);

    float left, top;
    if (!rec.isWorldSpace && world.HasComponent<RectangleComponent>(rec.entity)) {
        const auto& rect = world.GetComponent<RectangleComponent>(rec.entity);
        left = rec.x + (rect.width  - (float)textWidth)     * 0.5f;
        top  = rec.y + (rect.height - (float)scaledFontSize) * 0.5f;
    } else {
        left = rec.x - textWidth * 0.5f;
        top  = rec.y - scaledFontSize * 0.5f;
    }

    return testPos.x >= left && testPos.x <= left + textWidth &&
           testPos.y >= top  && testPos.y <= top + scaledFontSize;
}

// Script draw-command renderable types: value-backed (Has == nullptr), Render casts
// record.payload back to the concrete command struct. No Pick/Bounds — script draws
// remain unpickable.

static void RenderDrawCircleCmd(RenderContext& ctx, const RenderRecord& rec) {
    const auto& c = *static_cast<const DrawCircleCmd*>(rec.payload);
    ctx.DrawCircleLines(c.x, c.y, c.radius, c.color);
}
static void RenderDrawLineCmd(RenderContext& ctx, const RenderRecord& rec) {
    const auto& c = *static_cast<const DrawLineCmd*>(rec.payload);
    ctx.DrawLine(c.x1, c.y1, c.x2, c.y2, c.color);
}
static void RenderDrawRectCmd(RenderContext& ctx, const RenderRecord& rec) {
    const auto& c = *static_cast<const DrawRectCmd*>(rec.payload);
    ctx.DrawRectangle(c.x, c.y, c.width, c.height, c.color);
}
static void RenderDrawEllipseCmd(RenderContext& ctx, const RenderRecord& rec) {
    const auto& c = *static_cast<const DrawEllipseCmd*>(rec.payload);
    ctx.DrawEllipseLines(c.x, c.y, c.radiusH, c.radiusV, c.color);
}
static void RenderDrawTextCmd(RenderContext& ctx, const RenderRecord& rec) {
    const auto& c = *static_cast<const DrawTextCmd*>(rec.payload);
    ctx.DrawText(c.text.c_str(), c.x, c.y, c.fontSize, c.color);
}
static void RenderDrawPolygonCmd(RenderContext& ctx, const RenderRecord& rec) {
    const auto& c = *static_cast<const DrawPolygonCmd*>(rec.payload);
    std::vector<Vector2> triangles = TriangulatePolygon(c.points);
    ctx.DrawTriangleList(triangles, c.color);
}

Matrix RenderProjector::CalculateTransform(const CameraView& view, const SceneLayer& layer) {
    switch (layer.space) {
        case SceneLayerSpace::Screen2D: {
            if (!view.screenInWorld) return Matrix::Identity();
            SceneLayer worldLayer = layer;
            worldLayer.space = SceneLayerSpace::World2D;
            Matrix screenToWorld = Matrix::Scale({view.screenScale, view.screenScale, 1.0f}) *
                                   Matrix::Translation({view.screenOrigin.x, view.screenOrigin.y, 0.0f});
            return screenToWorld * CalculateTransform(view, worldLayer);
        }
        case SceneLayerSpace::World2D: {
            Vector2 viewportCenter = {
                view.viewport.width  * 0.5f,
                view.viewport.height * 0.5f
            };
            Matrix centerTranslation = Matrix::Translation({viewportCenter.x, viewportCenter.y, 0.0f});
            Matrix scale             = Matrix::Scale({view.zoom, view.zoom, 1.0f});
            Matrix cameraTranslation = Matrix::Translation({-view.position.x, -view.position.y, 0.0f});
            // Row-vector convention: v * (A * B * C) applies A first — must match WorldToFramebuffer below.
            return cameraTranslation * scale * centerTranslation;
        }
    }
    return Matrix::Identity();
}

Vector2 RenderProjector::WorldToFramebuffer(Vector2 worldPos, const CameraView& view) {
    Vector2 viewportCenter = { view.viewport.width * 0.5f, view.viewport.height * 0.5f };
    float zoom = view.zoom != 0.0f ? view.zoom : 1.0f;
    return {
        (worldPos.x - view.position.x) * zoom + viewportCenter.x,
        (worldPos.y - view.position.y) * zoom + viewportCenter.y
    };
}

Vector2 RenderProjector::FramebufferToWorld(Vector2 fbPos, const CameraView& view) {
    Vector2 viewportCenter = { view.viewport.width * 0.5f, view.viewport.height * 0.5f };
    float zoom = view.zoom != 0.0f ? view.zoom : 1.0f;
    return {
        (fbPos.x - viewportCenter.x) / zoom + view.position.x,
        (fbPos.y - viewportCenter.y) / zoom + view.position.y
    };
}

Vector2 RenderProjector::ScreenToFramebuffer(Vector2 screenPos, const CameraView& view) {
    if (!view.screenInWorld) return screenPos;
    return WorldToFramebuffer({ view.screenOrigin.x + screenPos.x * view.screenScale,
                                view.screenOrigin.y + screenPos.y * view.screenScale }, view);
}

Vector2 RenderProjector::FramebufferToScreen(Vector2 fbPos, const CameraView& view) {
    if (!view.screenInWorld) return fbPos;
    Vector2 world = FramebufferToWorld(fbPos, view);
    float scale = view.screenScale != 0.0f ? view.screenScale : 1.0f;
    return { (world.x - view.screenOrigin.x) / scale, (world.y - view.screenOrigin.y) / scale };
}

void RenderSorter::ComputeHiddenEntities(World& world) {
    ProfileN("RenderSorter ComputeHiddenEntities");
    hiddenEntities_.clear();

    // Seed with entities hidden via their own LayerComponent, then cascade down the
    // hierarchy so children without their own LayerComponent inherit it too.
    std::vector<Entity> stack;
    world.Query<LayerComponent>([&](Entity entity, const LayerComponent& layerComp) {
        if (!layerComp.isVisible || layerComp.inFog) stack.push_back(entity);
    });

    const auto& allChildren = world.GetAllChildren();
    while (!stack.empty()) {
        Entity entity = stack.back();
        stack.pop_back();
        if (!hiddenEntities_.insert(entity).second) continue;  // already visited

        auto it = allChildren.find(entity);
        if (it != allChildren.end()) {
            for (Entity child : it->second) stack.push_back(child);
        }
    }
}

void RenderSorter::CollectEntities(World& world, const CameraView& view) {
    ProfileN("Collect Renderables");
    auto& registry = RenderableRegistry::Instance();
    const auto& types = registry.All();

    Vector2 cameraPos = view.position;
    float halfW = (view.viewport.width  * 0.5f) / view.zoom;
    float halfH = (view.viewport.height * 0.5f) / view.zoom;

    float worldLeft   = cameraPos.x - halfW - 100;
    float worldRight  = cameraPos.x + halfW + 100;
    float worldTop    = cameraPos.y - halfH - 100;
    float worldBottom = cameraPos.y + halfH + 100;

    world.Query<TransformComponent>([&](Entity entity, auto& transform) {
        if (world.HasComponent<CameraComponent>(entity)) return;

        Vector2 pos = { transform.worldX, transform.worldY };

        constexpr size_t kMaxMatches = 32;
        RenderableTypeId matched[kMaxMatches];
        size_t matchedCount = 0;
        bool skipCull = false;

        for (size_t i = 0; i < types.size(); i++) {
            const auto& type = types[i];
            if (!type.Has || !type.Has(world, entity)) continue;
            if (matchedCount < kMaxMatches) matched[matchedCount++] = (RenderableTypeId)i;
            if (type.skipFrustumCull) skipCull = true;
        }
        if (matchedCount == 0) return;

        uint8_t layerIndex   = defaultLayerIndex_;
        uint8_t isWorldSpace = (layers_[defaultLayerIndex_].space == SceneLayerSpace::World2D) ? 1 : 0;
        bool isVisible = true;
        if (world.HasComponent<LayerComponent>(entity)) {
            const auto& layerComp = world.GetComponent<LayerComponent>(entity);
            auto it = layerNameToIndex_.find(layerComp.name);
            if (it != layerNameToIndex_.end()) {
                layerIndex   = it->second;
                isWorldSpace = (layers_[layerIndex].space == SceneLayerSpace::World2D) ? 1 : 0;
                isVisible    = layers_[layerIndex].isVisible;
            }
            // else: an unresolvable layer name falls back to defaultLayerIndex_ above — isWorldSpace
            // must match that same fallback, not stay hardcoded, or Pick()/gizmo projection breaks.
        }
        if (!isVisible) return;
        if (hiddenEntities_.contains(entity)) return;

        // Only world-space positions can be tested against the camera's view: a Screen2D
        // entity's position is in framebuffer pixels, which has nothing to do with where
        // the camera (the editor's free camera especially) happens to be looking.
        if (isWorldSpace && !skipCull &&
            (pos.x < worldLeft || pos.x > worldRight || pos.y < worldTop || pos.y > worldBottom))
            return;

        // Cached by TransformSystem alongside worldX/Y; clamp to the key's uint8_t width.
        uint8_t hierarchyDepth = (uint8_t)std::min<uint32_t>(transform.worldDepth, 255u);
        uint8_t childIndex = 0;
        if (world.HasComponent<ParentComponent>(entity)) {
            childIndex = (uint8_t)std::min<uint32_t>(world.GetComponent<ParentComponent>(entity).childIndex, 255u);
        }

        // One record per matched type — a Rectangle+Text entity produces two records.
        for (size_t k = 0; k < matchedCount; k++) {
            RenderRecord rec;
            rec.layerIndex = layerIndex;
            rec.hierarchyDepth = hierarchyDepth;
            rec.childIndex = childIndex;
            rec.isWorldSpace = isWorldSpace;
            rec.x = pos.x;
            rec.y = pos.y;
            rec.typeId = matched[k];
            rec.entity = entity;
            rec.payload = nullptr;
            rec.collectionOrder = nextCollectionOrder_++;
            queue_.push_back(rec);
        }
    });
}

void RenderSorter::CollectDrawCommands(const std::vector<DrawCommand>& drawCommands) {
    ProfileN("Collect Draw Commands");
    for (const auto& cmd : drawCommands) {
        std::visit([&](const auto& c) {
            using T = std::decay_t<decltype(c)>;

            auto it = layerNameToIndex_.find(c.layer);
            if (it == layerNameToIndex_.end()) return;  // unknown layer name, silently dropped
            uint8_t layerIndex = it->second;

            float rx = 0.0f, ry = 0.0f;
            if constexpr (std::is_same_v<T, DrawLineCmd>) {
                rx = c.x1; ry = c.y1;
            } else if constexpr (std::is_same_v<T, DrawPolygonCmd>) {
                if (!c.points.empty()) { rx = c.points[0].x; ry = c.points[0].y; }
            } else {
                rx = c.x; ry = c.y;
            }

            RenderRecord rec;
            rec.layerIndex = layerIndex;
            rec.hierarchyDepth = 255;  // defaults to drawing on top, above all entities in the layer
            rec.childIndex = 0;
            rec.isWorldSpace = (layers_[layerIndex].space == SceneLayerSpace::World2D) ? 1 : 0;
            rec.x = rx;
            rec.y = ry;
            rec.typeId = DrawCmdTypeId<T>();
            rec.entity = INVALID_ENTITY;
            rec.payload = &c;
            rec.collectionOrder = nextCollectionOrder_++;
            queue_.push_back(rec);
        }, cmd);
    }
}

void RenderSorter::SortQueue() {
    ProfileN("Sort Renderables");
    // Every key but the layer is per-entity, so an entity's records stay contiguous
    // whichever keys are switched off (RenderRecords relies on it).
    const SortOptions options = sortOptions_;
    std::sort(queue_.begin(), queue_.end(), [options](const RenderRecord& a, const RenderRecord& b) {
        if (a.layerIndex != b.layerIndex) return a.layerIndex < b.layerIndex;
        if (options.hierarchySort) {
            if (a.hierarchyDepth != b.hierarchyDepth) return a.hierarchyDepth < b.hierarchyDepth;
            if (a.childIndex != b.childIndex) return a.childIndex < b.childIndex;
        }
        // Y-sort only for World2D layers; ties (and Screen2D layers) fall back to collection order.
        if (options.ySort && a.isWorldSpace && a.y != b.y) return a.y < b.y;
        return a.collectionOrder < b.collectionOrder;
    });
}

void RenderSorter::Build(World& world, Scene& scene, const std::vector<DrawCommand>& drawCommands, const CameraView& view) {
    ProfileN("RenderSorter Build");
    ComputeHiddenEntities(world);

    // Sorted by zIndex (stable, so equal-zIndex layers keep declaration order).
    layers_ = scene.GetLayers();
    std::stable_sort(layers_.begin(), layers_.end(), [](const SceneLayer& a, const SceneLayer& b) {
        return a.zIndex < b.zIndex;
    });
    // The editor's hide/solo, applied to this local copy only — the scene's own SceneLayer
    // flags are never touched, so hiding a layer to work on another can't be saved into the file.
    for (auto& layer : layers_) {
        if (hiddenLayerOverride_.contains(layer.name)) layer.isVisible = false;
    }

    layerNameToIndex_.clear();
    layerNameToIndex_.reserve(layers_.size());
    for (size_t i = 0; i < layers_.size(); i++) {
        layerNameToIndex_[layers_[i].name] = (uint8_t)i;
    }

    defaultLayerIndex_ = 0;
    auto defaultIt = layerNameToIndex_.find("default");
    if (defaultIt != layerNameToIndex_.end()) {
        defaultLayerIndex_ = defaultIt->second;
    }

    queue_.clear();
    nextCollectionOrder_ = 0;

    CollectEntities(world, view);
    CollectDrawCommands(drawCommands);
    SortQueue();
}

const Framebuffer& RenderCompositor::EnsureCompositeBuffer(int width, int height) {
    compositeBuffer_.Resize(width, height);
    return compositeBuffer_;
}

void RenderCompositor::PushBlend(RenderContext& ctx, SceneLayerBlend blend) {
    switch (blend) {
        case SceneLayerBlend::Additive: ctx.PushBlendMode(BlendMode::Additive);       break;
        case SceneLayerBlend::Multiply: ctx.PushBlendMode(BlendMode::Multiplicative); break;
        default:                        ctx.PushBlendMode(BlendMode::Alpha);          break;
    }
}

const Framebuffer& RenderCompositor::EnsureEntityBuffer(Entity entity, int width, int height) {
    shadedThisFrame_.insert(entity);
    Framebuffer& buffer = entityBuffers_[entity];
    buffer.Resize(width, height);
    return buffer;
}

void RenderCompositor::PruneEntityBuffers() {
    for (auto it = entityBuffers_.begin(); it != entityBuffers_.end();) {
        if (shadedThisFrame_.count(it->first)) {
            ++it;
        } else {
            it = entityBuffers_.erase(it);
        }
    }
    shadedThisFrame_.clear();
}

template <typename T>
static bool IsEnabled(const World& world, Entity entity) {
    return entity != INVALID_ENTITY && world.HasComponent<T>(entity) && world.GetComponent<T>(entity).enabled;
}

void RenderCompositor::RenderRecords(RenderContext& ctx, std::span<const RenderRecord> records,
                                     const Matrix& layerTransform, const Framebuffer& enclosingTarget) {
    ProfileN("Render Records");
    const World& world = ctx.GetWorld();

    size_t index = 0;
    while (index < records.size()) {
        const Entity entity = records[index].entity;

        // Every record an entity produced is contiguous here: the sort keys ahead of
        // collectionOrder (layer, hierarchyDepth, childIndex, y) are all per-entity, so
        // two entities' records can never interleave.
        size_t end = index + 1;
        if (entity != INVALID_ENTITY) {
            while (end < records.size() && records[end].entity == entity) ++end;
        }

        std::span<const RenderRecord> group = records.subspan(index, end - index);
        index = end;

        // Normals and emission come from materials alone; anything else (text, tiles,
        // script draws) leaves the flat, dark defaults the buffers were cleared to.
        if (output_ != SurfaceOutput::Color) {
            if (IsEnabled<MaterialComponent>(world, entity)) RenderMaterialEntity(ctx, entity, group);
            continue;
        }

        // A ShaderComponent filters the entity after it's drawn: RenderShadedEntity draws
        // it (materials included) into an offscreen buffer and blits that through the shader.
        if (IsEnabled<ShaderComponent>(world, entity)) {
            RenderShadedEntity(ctx, entity, group, layerTransform, enclosingTarget);
        } else {
            RenderEntity(ctx, entity, group);
        }
    }
}

void RenderCompositor::RenderEntity(RenderContext& ctx, Entity entity, std::span<const RenderRecord> records) {
    if (IsEnabled<MaterialComponent>(ctx.GetWorld(), entity)) {
        RenderMaterialEntity(ctx, entity, records);
        return;
    }
    auto& registry = RenderableRegistry::Instance();
    for (const auto& rec : records) {
        const RenderableType& type = registry.Get(rec.typeId);
        if (type.Render) type.Render(ctx, rec);
    }
}

// Every declared non-built-in uniform at its default unless overridden. Set
// unconditionally rather than diffed: a Shader is shared by every entity/layer using it,
// so the previous draw's values are still resident.
static void ApplyUniformOverrides(Shader& shader, const std::unordered_map<std::string, Value>& overrides) {
    for (const ShaderUniform& uniform : shader.GetUniforms()) {
        if (uniform.isBuiltIn) continue;
        auto overrideIt = overrides.find(uniform.name);
        const bool useOverride = overrideIt != overrides.end() &&
                                 overrideIt->second.SameType(uniform.defaultValue);
        shader.SetUniform(uniform.name, useOverride ? overrideIt->second : uniform.defaultValue);
    }
}

// Composed shaders are requested lazily on first use. Remembered so a load in flight
// (or one that failed — a missing chunk) isn't re-requested every frame.
Shader* RenderCompositor::GetComposedShader(Services::IAssetService& assets, const char* geometry, const std::string& material) {
    const char* output = "";
    switch (output_) {
        case SurfaceOutput::Normal:   output = "Normal";   break;
        case SurfaceOutput::Emission: output = "Emission"; break;
        case SurfaceOutput::Height:   output = "Height";   break;
        case SurfaceOutput::Color:    break;
    }
    return GetShader(assets, ComposedShaderPath(geometry, material, output));
}

Shader* RenderCompositor::GetShader(Services::IAssetService& assets, const Path& path) {
    if (Shader* shader = assets.Get<Shader>(path)) return shader->IsValid() ? shader : nullptr;
    if (requestedShaders_.insert(path.GetRelativePath()).second) assets.LoadAsset<Shader>(path);
    return nullptr;
}

void RenderCompositor::RenderMaterialEntity(RenderContext& ctx, Entity entity, std::span<const RenderRecord> records) {
    ProfileN("Render Material Entity");

    auto& registry = RenderableRegistry::Instance();
    const World& world = ctx.GetWorld();
    auto& assets = ctx.GetServices().Get<Elysium::Services::IAssetService>();
    const MaterialComponent& material = world.GetComponent<MaterialComponent>(entity);
    const float padding = std::max(0.0f, material.padding);
    const float time = (float)::GetTime();

    struct Pass {
        Shader* shader;
        const MaterialLayer* layer;
    };
    std::vector<Pass> passes;

    for (const auto& rec : records) {
        const RenderableType& type = registry.Get(rec.typeId);

        SdfGeometry geometry;
        if (!type.Geometry || !type.Geometry(world, rec, geometry) || !geometry.geometry) {
            // Text on a button, a Sprite, ... Its colors aren't heights: it stays on the ground.
            if (type.Render && output_ != SurfaceOutput::Height) type.Render(ctx, rec);
            continue;
        }

        // Resolve every layer's shader up front. Any still loading -> skip the shape this
        // frame rather than draw a partial stack (e.g. a stroke with no fill).
        passes.clear();
        bool ready = true;
        for (const MaterialLayer& layer : material.layers) {
            if (!layer.enabled) continue;
            Shader* shader = GetComposedShader(assets, geometry.geometry, layer.material);
            if (!shader) { ready = false; continue; }
            passes.push_back({shader, &layer});
        }
        if (!ready || passes.empty()) continue;

        const Rectangle& box = geometry.box;
        Rectangle quad = { box.x - padding, box.y - padding, box.width + padding * 2.0f, box.height + padding * 2.0f };

        // The shaders work in quad-local space, so transforming the quad's corners scales,
        // rotates and mirrors the shape and every effect around it together. Coverage
        // is fwidth-based, so edges stay one pixel wide at any scale.
        const ShapeTransform xf = GetShapeTransform(world, rec);
        const Vector2 corners[4] = {
            xf.ToDraw(Vector2{quad.x, quad.y}),
            xf.ToDraw(Vector2{quad.x, quad.y + quad.height}),
            xf.ToDraw(Vector2{quad.x + quad.width, quad.y + quad.height}),
            xf.ToDraw(Vector2{quad.x + quad.width, quad.y}),
        };

        for (const Pass& pass : passes) {
            Shader& shader = *pass.shader;
            shader.SetUniform("e_Time", Value{time});
            shader.SetUniform("e_Size", Value{Vector2{box.width, box.height}});
            shader.SetUniform("e_QuadSize", Value{Vector2{quad.width, quad.height}});
            shader.SetUniform("e_CornerRadius", Value{geometry.cornerRadius});
            shader.SetUniform("e_PointA", Value{geometry.pointA});
            shader.SetUniform("e_PointB", Value{geometry.pointB});
            shader.SetUniform("e_Thickness", Value{geometry.thickness});
            if (!geometry.points.empty()) {
                shader.SetUniform("e_PointCount", Value{(int)geometry.points.size()});
                shader.SetFloatArray("e_Points", &geometry.points[0].x, (int)geometry.points.size(), 2);
            }
            ApplyUniformOverrides(shader, pass.layer->overrides);

            // Lit layers: how normals turn with the shape, and its lighting maps.
            const Texture* normalMap = nullptr;
            const Texture* emissionMap = nullptr;
            if (output_ != SurfaceOutput::Color) {
                shader.SetUniform("e_NormalXform", Value{Vector4{xf.cs, xf.sn, xf.scaleX < 0.0f ? -1.0f : 1.0f,
                                                                 xf.scaleY < 0.0f ? -1.0f : 1.0f}});
                auto mapOf = [&](const std::string& path) -> const Texture* {
                    const Texture* map = path.empty() ? nullptr : assets.Get<Texture>(Path(path));
                    return map && map->id != 0 ? map : nullptr;
                };
                normalMap = mapOf(pass.layer->normalMapPath);
                emissionMap = mapOf(pass.layer->emissionMapPath);
                shader.SetUniform("e_HasNormalMap", Value{normalMap != nullptr});
                shader.SetUniform("e_HasEmissionMap", Value{emissionMap != nullptr});
            }

            const Texture* texture = pass.layer->texturePath.empty()
                                         ? nullptr
                                         : assets.Get<Texture>(Path(pass.layer->texturePath));
            if (texture && texture->id == 0) texture = nullptr;
            if (texture) shader.SetUniform("e_TextureSize", Value{Vector2{(float)texture->width, (float)texture->height}});

            // The height pass: where the quad is in the world, and the line it stands on.
            if (output_ == SurfaceOutput::Height) {
                GroundLine line{Vector2{rec.x, rec.y}, 0.0f};
                if (auto it = groundLines_.find(entity); it != groundLines_.end()) {
                    line = it->second;
                } else if (world.HasComponent<TransformComponent>(entity)) {
                    const auto& transform = world.GetComponent<TransformComponent>(entity);
                    line.anchor = {transform.worldX, transform.worldY};
                }
                shader.SetUniform("e_QuadCorner", Value{corners[0]});
                shader.SetUniform("e_QuadAxisU", Value{Vector2{corners[3].x - corners[0].x, corners[3].y - corners[0].y}});
                shader.SetUniform("e_QuadAxisV", Value{Vector2{corners[1].x - corners[0].x, corners[1].y - corners[0].y}});
                shader.SetUniform("e_GroundAnchor", Value{line.anchor});
                shader.SetUniform("e_GroundSlope", Value{line.slope});
                shader.SetUniform("e_GroundFlat", Value{line.flat ? 1.0f : 0.0f});
            }

            ctx.PushShader(shader);
            // Samplers bind to the draw that follows, so only once the shader is pushed.
            if (normalMap) shader.SetTexture("e_NormalMap", normalMap->id);
            if (emissionMap) shader.SetTexture("e_EmissionMap", emissionMap->id);
            ctx.DrawShaderQuad(corners, texture, Colors::White);
            ctx.PopShader();
        }
    }
}

void RenderCompositor::RenderShadedEntity(RenderContext& ctx, Entity entity,
                                          std::span<const RenderRecord> records, const Matrix& layerTransform,
                                          const Framebuffer& enclosingTarget) {
    ProfileN("Render Shaded Entity");

    auto& registry = RenderableRegistry::Instance();
    const World& world = ctx.GetWorld();
    const ShaderComponent& shaderComponent = world.GetComponent<ShaderComponent>(entity);

    // Used for every bail-out below: a shader still loading, a bad size, or a dead
    // framebuffer must degrade to the plain unshaded draw, never to a missing entity.
    auto RenderUnshaded = [&] { RenderEntity(ctx, entity, records); };

    if (records.empty()) return;

    auto& assets = ctx.GetServices().Get<Elysium::Services::IAssetService>();
    Shader* shader = shaderComponent.shaderPath.empty()
                         ? nullptr
                         : assets.Get<Shader>(Path(shaderComponent.shaderPath));
    if (!shader || !shader->IsValid()) {
        RenderUnshaded();
        return;
    }

    // Union of whatever bounds the entity's renderables report; components without a
    // Bounds callback (Text, Line, ...) fall back to the component's authored box.
    std::optional<Rectangle> bounds;
    for (const auto& rec : records) {
        const RenderableType& type = registry.Get(rec.typeId);
        if (!type.Bounds) continue;
        std::optional<Rectangle> recordBounds = type.Bounds(world, rec);
        if (!recordBounds) continue;
        bounds = bounds ? Union(*bounds, *recordBounds) : *recordBounds;
    }
    if (!bounds) {
        bounds = Rectangle{records[0].x - shaderComponent.width * 0.5f,
                           records[0].y - shaderComponent.height * 0.5f,
                           shaderComponent.width, shaderComponent.height};
    }

    const float padding = std::max(0.0f, shaderComponent.padding);
    Rectangle region = {bounds->x - padding, bounds->y - padding,
                        bounds->width + padding * 2.0f, bounds->height + padding * 2.0f};

    // 1 world unit : 1 texel. Resolution-independent enough for a 2D effect, and it keeps
    // the offscreen size stable as the camera zooms.
    constexpr int kMaxEntityBufferSize = 4096;
    int bufferWidth  = (int)std::ceil(region.width);
    int bufferHeight = (int)std::ceil(region.height);
    if (bufferWidth <= 0 || bufferHeight <= 0 ||
        bufferWidth > kMaxEntityBufferSize || bufferHeight > kMaxEntityBufferSize) {
        RenderUnshaded();
        return;
    }

    const Framebuffer& buffer = EnsureEntityBuffer(entity, bufferWidth, bufferHeight);
    if (!buffer.IsValid()) {
        RenderUnshaded();
        return;
    }

    // Switching render targets resets rlgl's modelview and clips to the target, so the
    // layer transform and the viewport scissor are torn down around the detour and rebuilt
    // afterwards — the same dance RenderComposited does for a whole layer.
    ctx.PopMatrix();
    // The clip is in window coordinates and would cut into the offscreen buffer, but who
    // pushed it depends on the path in (RenderComposited already popped it), so save and
    // restore exactly what was there rather than assuming.
    const bool hadScissor = ctx.HasScissor();
    Rectangle savedScissor{};
    if (hadScissor) {
        savedScissor = ctx.CurrentScissor();
        ctx.PopScissorMode();
    }

    ctx.BeginRenderTarget(buffer);
    ctx.ClearTarget(Colors::Transparent);
    ctx.PushMatrix();
    ctx.Translate(-region.x, -region.y, 0.0f);  // entity world space -> buffer texel space
    RenderUnshaded();
    ctx.PopMatrix();
    ctx.EndRenderTarget();
    ctx.BeginRenderTarget(enclosingTarget);  // EndTextureMode drops to the backbuffer

    if (hadScissor) {
        ctx.PushScissorMode((int)savedScissor.x, (int)savedScissor.y,
                            (int)savedScissor.width, (int)savedScissor.height);
    }
    ctx.PushMatrix();
    ctx.MultiplyMatrix(layerTransform);

    // Engine-fed uniforms first, then every declared uniform at its default or override.
    shader->SetUniform("e_Time", Value{(float)::GetTime()});
    shader->SetUniform("e_Resolution", Value{Vector2{(float)bufferWidth, (float)bufferHeight}});
    shader->SetUniform("e_TexelSize", Value{Vector2{1.0f / bufferWidth, 1.0f / bufferHeight}});
    ApplyUniformOverrides(*shader, shaderComponent.overrides);

    ctx.PushShader(*shader);
    ctx.DrawFramebuffer(buffer, region, Colors::White);
    ctx.PopShader();
}

void RenderCompositor::RenderLayer(RenderContext& ctx, const CameraView& view,
                                    const SceneLayer& layer, std::span<const RenderRecord> records) {
    if (layer.isLit) {
        RenderLit(ctx, view, layer, records);
    } else if (layer.isComposited) {
        RenderComposited(ctx, view, layer, records);
    } else {
        RenderImmediate(ctx, view, layer, records);
    }
}

void RenderCompositor::RenderImmediate(RenderContext& ctx, const CameraView& view,
                                        const SceneLayer& layer, std::span<const RenderRecord> records) {
    ProfileN("Render Immediate Layer");

    if (layer.opacity < 1.0f) {
        // A faded layer must be alpha-blended as a single unit, even if not flagged isComposited.
        RenderComposited(ctx, view, layer, records);
        return;
    }

    Matrix viewProjectionTransform = RenderProjector::CalculateTransform(view, layer);
    PushBlend(ctx, layer.layerBlend);
    ctx.PushMatrix();
    ctx.MultiplyMatrix(viewProjectionTransform);

    auto& sceneService = ctx.GetServices().Get<Services::ISceneService>();
    RenderRecords(ctx, records, viewProjectionTransform, sceneService.GetFramebuffer());

    ctx.PopMatrix();
    ctx.PopBlendMode();
}

void RenderCompositor::RenderComposited(RenderContext& ctx, const CameraView& view,
                                         const SceneLayer& layer, std::span<const RenderRecord> records) {
    ProfileN("Render Composited Layer");

    Matrix layerTransform = RenderProjector::CalculateTransform(view, layer);

    int w = (int)view.viewport.width;
    int h = (int)view.viewport.height;
    const Framebuffer& compositionBuffer = EnsureCompositeBuffer(w, h);

    ctx.PopScissorMode();

    ctx.BeginRenderTarget(compositionBuffer);
    ctx.ClearTarget(layer.ambient);

    PushBlend(ctx, layer.layerBlend);
    ctx.PushMatrix();
    ctx.MultiplyMatrix(layerTransform);

    RenderRecords(ctx, records, layerTransform, compositionBuffer);

    ctx.PopMatrix();
    ctx.PopBlendMode();

    ctx.EndRenderTarget();

    // Restore SceneService's framebuffer
    auto& sceneService = ctx.GetServices().Get<Services::ISceneService>();
    ctx.BeginRenderTarget(sceneService.GetFramebuffer());

    PushBlend(ctx, layer.compositeBlend);
    Rectangle dst = {view.viewport.x, view.viewport.y, (float)w, (float)h};
    Color compositeTint = Colors::White;
    compositeTint.a = (unsigned char)(255.0f * std::clamp(layer.opacity, 0.0f, 1.0f));
    ctx.DrawFramebuffer(compositionBuffer, dst, compositeTint);

    ctx.PopBlendMode();

    ctx.PushScissorMode(
        (int)view.viewport.x,
        (int)view.viewport.y,
        (int)view.viewport.width,
        (int)view.viewport.height);
}

// A layer's buffers cover the viewport; this is the stretch of the layer's own space (the
// world, for World2D) they show.
struct LayerRegion {
    Vector2 origin;  // at the buffers' top-left
    Vector2 size;
};

static LayerRegion GetLayerRegion(const CameraView& view, const SceneLayer& layer, int width, int height) {
    if (layer.space == SceneLayerSpace::Screen2D && !view.screenInWorld) {
        return {Vector2{0.0f, 0.0f}, Vector2{(float)width, (float)height}};
    }
    const float zoom = view.zoom != 0.0f ? view.zoom : 1.0f;
    Vector2 origin = RenderProjector::FramebufferToWorld(Vector2{0.0f, 0.0f}, view);
    Vector2 size{width / zoom, height / zoom};
    if (layer.space == SceneLayerSpace::Screen2D) {
        const float scale = view.screenScale != 0.0f ? view.screenScale : 1.0f;
        origin = {(origin.x - view.screenOrigin.x) / scale, (origin.y - view.screenOrigin.y) / scale};
        size = {size.x / scale, size.y / scale};
    }
    return {origin, size};
}

const Framebuffer& RenderCompositor::BuildOccluderField(RenderContext& ctx, Shader& flood, Vector2 origin, Vector2 size,
                                                        int gridWidth, int gridHeight, bool shadows) {
    ProfileN("Build Occluder Field");

    // Footprints, one texel per ground texel, carrying their height in 8 bits: r is
    // height / 2040 (LightGather.fs scales it back), 8 world units a step.
    occluderMask_.Resize(gridWidth, gridHeight);
    ctx.BeginRenderTarget(occluderMask_);
    ctx.ClearTarget(Colors::Transparent);
    if (shadows) {
        ctx.PushBlendMode(BlendMode::Alpha);
        ctx.PushMatrix();
        ctx.Scale(gridWidth / size.x, gridHeight / size.y, 1.0f);
        ctx.Translate(-origin.x, -origin.y, 0.0f);
        for (const ShadowCaster& caster : shadowCasters_) {
            if (caster.footprint.size() < 3 || caster.height <= 0.0f) continue;
            const float level = std::clamp(std::ceil(caster.height / 8.0f), 1.0f, 255.0f);
            ctx.DrawTriangleList(TriangulatePolygon(caster.footprint), Color{(unsigned char)level, 0, 0, 255});
        }
        ctx.PopMatrix();
        ctx.PopBlendMode();
    }
    ctx.EndRenderTarget();

    // Jump flood: seed from the mask, then halve the step down to one texel.
    floodA_.Resize(gridWidth, gridHeight);
    floodB_.Resize(gridWidth, gridHeight);
    const Rectangle grid{0.0f, 0.0f, (float)gridWidth, (float)gridHeight};
    flood.SetUniform("e_GridSize", Value{Vector2{(float)gridWidth, (float)gridHeight}});
    auto pass = [&](const Framebuffer& source, const Framebuffer& target, int mode, int step) {
        flood.SetUniform("e_Mode", Value{mode});
        flood.SetUniform("e_Step", Value{(float)step});
        ctx.BeginRenderTarget(target);
        ctx.PushBlendMode(BlendMode::Alpha);  // opaque output: a plain write
        ctx.PushShader(flood);
        ctx.DrawFramebuffer(source, grid, Colors::White);
        ctx.PopShader();
        ctx.PopBlendMode();
        ctx.EndRenderTarget();
    };
    pass(occluderMask_, floodA_, 0, 0);
    if (!shadows) return floodA_;
    const Framebuffer* source = &floodA_;
    const Framebuffer* target = &floodB_;
    int step = 1;
    while (step * 2 < std::max(gridWidth, gridHeight)) step *= 2;
    for (; step >= 1; step /= 2) {
        pass(*source, *target, 1, step);
        std::swap(source, target);
    }
    return *source;
}

void RenderCompositor::RenderShadowAtlas(RenderContext& ctx) {
    if (!shadowAtlasDirty_) return;
    shadowAtlasDirty_ = false;
    ProfileN("Render Shadow Atlas");

    // Every caster's footprint extruded to its height: its walls and its lid.
    std::vector<Vector3> triangles;
    std::vector<unsigned int> owners;
    auto ownerOf = [](Entity e) {
        return e == INVALID_ENTITY ? ShadowAtlas::kNoOwner : (unsigned int)e;
    };
    for (const ShadowCaster& caster : shadowCasters_) {
        const auto& footprint = caster.footprint;
        if (footprint.size() < 3 || caster.height <= 0.0f) continue;
        for (size_t i = 0; i < footprint.size(); ++i) {
            const Vector2 a = footprint[i], b = footprint[(i + 1) % footprint.size()];
            const Vector3 a0 = WorldTo3D(a, 0.0f), b0 = WorldTo3D(b, 0.0f);
            const Vector3 a1 = WorldTo3D(a, caster.height), b1 = WorldTo3D(b, caster.height);
            triangles.insert(triangles.end(), {a0, b0, b1, a0, b1, a1});
        }
        for (const Vector2& p : TriangulatePolygon(footprint)) triangles.push_back(WorldTo3D(p, caster.height));
        owners.resize(triangles.size() / 3, ownerOf(caster.owner));
    }

    std::vector<ShadowAtlas::Light> lights;
    for (const PointLight& light : lights_) lights.push_back({light.position, light.radius, ownerOf(light.owner)});
    shadowAtlas_.Render(ctx, lights, triangles, owners);
}

void RenderCompositor::RenderLit(RenderContext& ctx, const CameraView& view,
                                  const SceneLayer& layer, std::span<const RenderRecord> records) {
    ProfileN("Render Lit Layer");

    auto& assets = ctx.GetServices().Get<Elysium::Services::IAssetService>();
    Shader* lighting = GetShader(assets, Path("Shaders/Lighting.fs", PathRoot::Engine));
    Shader* gather = GetShader(assets, Path("Shaders/LightGather.fs", PathRoot::Engine));
    Shader* flood = GetShader(assets, Path("Shaders/OccluderField.fs", PathRoot::Engine));
    Shader* grounding = GetShader(assets, Path("Shaders/GroundEmission.fs", PathRoot::Engine));
    if (!lighting || !gather || !flood || !grounding) {
        // Still compiling (or failed): the layer unlit rather than missing.
        RenderComposited(ctx, view, layer, records);
        return;
    }

    const Matrix layerTransform = RenderProjector::CalculateTransform(view, layer);
    const int w = (int)view.viewport.width;
    const int h = (int)view.viewport.height;
    const Framebuffer& albedo = EnsureCompositeBuffer(w, h);
    normalBuffer_.Resize(w, h);
    emissionBuffer_.Resize(w, h);
    heightBuffer_.Resize(w, h);

    ctx.PopScissorMode();

    // The four surfaces. Normals clear to flat (facing the camera), emission to none,
    // height to the ground.
    struct Surface { SurfaceOutput output; const Framebuffer* target; Color clear; };
    const Surface surfaces[] = {
        {SurfaceOutput::Color, &albedo, layer.ambient},
        {SurfaceOutput::Normal, &normalBuffer_, Color{128, 128, 255, 0}},
        {SurfaceOutput::Emission, &emissionBuffer_, Color{0, 0, 0, 0}},
        {SurfaceOutput::Height, &heightBuffer_, Color{0, 0, 0, 0}},
    };
    for (const Surface& surface : surfaces) {
        output_ = surface.output;
        ctx.BeginRenderTarget(*surface.target);
        ctx.ClearTarget(surface.clear);
        // The layer's blend is for its colors; normals and glow simply stack, and heights
        // are written opaque (the last thing drawn over a pixel wins).
        PushBlend(ctx, surface.output == SurfaceOutput::Color ? layer.layerBlend : SceneLayerBlend::Normal);
        ctx.PushMatrix();
        ctx.MultiplyMatrix(layerTransform);
        RenderRecords(ctx, records, layerTransform, *surface.target);
        ctx.PopMatrix();
        ctx.PopBlendMode();
        ctx.EndRenderTarget();
    }
    output_ = SurfaceOutput::Color;

    // The ground the light is worked out over: what the layer shows, plus a margin so
    // walls and glows just off screen still cast and light, and so the ground under tall
    // things at the bottom edge is covered. Two grids over the same region: occluders and
    // ground emission at half the layer's resolution, the light at a quarter (it's smooth,
    // and the gather is the expensive part).
    const LayerRegion region = GetLayerRegion(view, layer, w, h);
    const float pixelsPerUnit = w / std::max(region.size.x, 1e-3f);
    constexpr float kMargin = 128.0f;  // world units
    constexpr int kMaxGrid = 2048;     // the flood stores texel coordinates in half floats
    Vector2 groundSize{region.size.x + kMargin * 2.0f, region.size.y + kMargin * 2.0f};
    const float texelWorld = std::max({2.0f / pixelsPerUnit, groundSize.x / kMaxGrid, groundSize.y / kMaxGrid});
    const int gw = std::max(1, (int)std::ceil(groundSize.x / texelWorld));
    const int gh = std::max(1, (int)std::ceil(groundSize.y / texelWorld));
    groundSize = {gw * texelWorld, gh * texelWorld};  // square texels
    const Vector2 groundOrigin{region.origin.x - kMargin, region.origin.y - kMargin};
    const int lw = std::max(1, gw / 2), lh = std::max(1, gh / 2);

    const bool shadows = layer.shadows && layer.space == SceneLayerSpace::World2D && !shadowCasters_.empty();
    const Framebuffer* field = nullptr;
    if (layer.pointLights) {
        // Light comes from LightComponents instead; their shadows are shared by every
        // lit layer and rendered once a frame.
        RenderShadowAtlas(ctx);
    } else {
        field = &BuildOccluderField(ctx, *flood, groundOrigin, groundSize, gw, gh, shadows);

        // Emission moves down to where it stands: a sample up the column every ground texel,
        // as high as anything is likely drawn.
        emissionBuffer_.GenerateMipmaps();
        heightBuffer_.SetLinearFilter(false);
        constexpr float kMaxLift = 192.0f;  // world units
        constexpr int kMaxLiftSamples = 96;
        const float liftStep = std::max(texelWorld, kMaxLift / kMaxLiftSamples);
        groundEmission_.Resize(gw, gh);
        grounding->SetUniform("e_GroundOrigin", Value{groundOrigin});
        grounding->SetUniform("e_GroundSize", Value{groundSize});
        grounding->SetUniform("e_ViewOrigin", Value{region.origin});
        grounding->SetUniform("e_ViewSize", Value{region.size});
        grounding->SetUniform("e_Step", Value{liftStep});
        grounding->SetUniform("e_Lod", Value{std::log2(std::max(liftStep * pixelsPerUnit, 1.0f))});
        grounding->SetUniform("e_Samples", Value{(int)std::ceil(kMaxLift / liftStep) + 1});
        ctx.BeginRenderTarget(groundEmission_);
        ctx.ClearTarget(Colors::Transparent);
        ctx.PushBlendMode(BlendMode::Alpha);
        ctx.PushShader(*grounding);
        grounding->SetTexture("e_HeightBuffer", heightBuffer_.TextureId());
        ctx.DrawFramebuffer(emissionBuffer_, Rectangle{0, 0, (float)gw, (float)gh}, Colors::White);
        ctx.PopShader();
        ctx.PopBlendMode();
        ctx.EndRenderTarget();
        // Blurred copies of it (its mips) are what the gather reads further away.
        groundEmission_.GenerateMipmaps();

        // Gather the light across the ground: how much arrives, then from where. Lighting.fs
        // re-aims it by each pixel's normal at full resolution.
        lightBuffer_.Resize(lw, lh);
        directionBuffer_.Resize(lw, lh);
        lightBuffer_.SetLinearFilter(true);
        directionBuffer_.SetLinearFilter(true);
        occluderMask_.SetLinearFilter(false);
        gather->SetUniform("e_GroundSize", Value{groundSize});
        gather->SetUniform("e_GridSize", Value{Vector2{(float)gw, (float)gh}});
        gather->SetUniform("e_Reach", Value{layer.lightReach});
        gather->SetUniform("e_Height", Value{layer.lightHeight});
        gather->SetUniform("e_Strength", Value{layer.lightStrength});
        gather->SetUniform("e_Shadows", Value{shadows});
        const Framebuffer* gathered[] = {&lightBuffer_, &directionBuffer_};
        for (int mode = 0; mode < 2; ++mode) {
            gather->SetUniform("e_Mode", Value{mode});
            ctx.BeginRenderTarget(*gathered[mode]);
            ctx.ClearTarget(Colors::Transparent);
            ctx.PushBlendMode(BlendMode::Alpha);  // opaque output: a plain write
            ctx.PushShader(*gather);
            gather->SetTexture("e_OccluderMask", occluderMask_.TextureId());
            gather->SetTexture("e_OccluderField", field->TextureId());
            ctx.DrawFramebuffer(groundEmission_, Rectangle{0, 0, (float)lw, (float)lh}, Colors::White);
            ctx.PopShader();
            ctx.PopBlendMode();
            ctx.EndRenderTarget();
        }
    }

    // Combine onto the frame, the way RenderComposited composites.
    auto rgb = [](Color c) { return Vector3{c.r / 255.0f, c.g / 255.0f, c.b / 255.0f}; };
    lighting->SetUniform("e_Resolution", Value{Vector2{(float)w, (float)h}});
    lighting->SetUniform("e_Ambient", Value{rgb(layer.lightAmbient)});
    lighting->SetUniform("e_Bands", Value{(float)layer.lightBands});
    lighting->SetUniform("e_Outline", Value{layer.outline});
    const Color ink = layer.outlineColor;
    lighting->SetUniform("e_OutlineColor", Value{Vector4{ink.r / 255.0f, ink.g / 255.0f, ink.b / 255.0f, ink.a / 255.0f}});
    lighting->SetUniform("e_ViewOrigin", Value{region.origin});
    lighting->SetUniform("e_ViewSize", Value{region.size});
    lighting->SetUniform("e_GroundOrigin", Value{groundOrigin});
    lighting->SetUniform("e_GroundSize", Value{groundSize});
    lighting->SetUniform("e_GridSize", Value{Vector2{(float)gw, (float)gh}});
    lighting->SetUniform("e_Shadows", Value{shadows});
    lighting->SetUniform("e_Debug", Value{layer.lightDebug});
    lighting->SetUniform("e_PointLights", Value{layer.pointLights});
    if (layer.pointLights) {
        const int count = shadowAtlas_.LightCount();
        std::vector<float> positions, colors, radii, vision;
        for (int i = 0; i < count; ++i) {
            const PointLight& light = lights_[i];
            positions.insert(positions.end(), {light.position.x, light.position.y, light.position.z});
            colors.insert(colors.end(), {light.color.x, light.color.y, light.color.z});
            radii.push_back(light.radius);
            vision.push_back(light.vision ? 1.0f : 0.0f);
        }
        lighting->SetUniform("e_Fog", Value{layer.fogOfWar});
        lighting->SetUniform("e_FogColor", Value{rgb(layer.fogColor)});
        lighting->SetUniform("e_LightCount", Value{count});
        lighting->SetUniform("e_ShadowBias", Value{layer.shadowBias});
        lighting->SetUniform("e_ShadowRows", Value{(float)ShadowAtlas::kMaxLights});
        lighting->SetUniform("e_ShadowTile", Value{(float)ShadowAtlas::kTileSize});
        if (count > 0) {
            lighting->SetFloatArray("e_LightPos", positions.data(), count, 3);
            lighting->SetFloatArray("e_LightColor", colors.data(), count, 3);
            lighting->SetFloatArray("e_LightRadius", radii.data(), count, 1);
            lighting->SetFloatArray("e_LightVision", vision.data(), count, 1);
        }
    }

    auto& sceneService = ctx.GetServices().Get<Services::ISceneService>();
    ctx.BeginRenderTarget(sceneService.GetFramebuffer());
    PushBlend(ctx, layer.compositeBlend);
    Color tint = Colors::White;
    tint.a = (unsigned char)(255.0f * std::clamp(layer.opacity, 0.0f, 1.0f));
    ctx.PushShader(*lighting);
    lighting->SetTexture("e_NormalBuffer", normalBuffer_.TextureId());
    lighting->SetTexture("e_EmissionBuffer", emissionBuffer_.TextureId());
    lighting->SetTexture("e_LightBuffer", lightBuffer_.TextureId());
    lighting->SetTexture("e_DirectionBuffer", directionBuffer_.TextureId());
    // Past the four samplers raylib's batch binds: straight to units of their own.
    lighting->SetTextureUnit("e_HeightBuffer", heightBuffer_.TextureId(), 8);
    lighting->SetTextureUnit("e_OccluderMask", occluderMask_.TextureId(), 9);
    if (field) lighting->SetTextureUnit("e_OccluderField", field->TextureId(), 10);
    lighting->SetTextureUnit("e_GroundEmission", groundEmission_.TextureId(), 11);
    if (layer.pointLights) lighting->SetTextureUnit("e_ShadowAtlas", shadowAtlas_.TextureId(), 12);
    ctx.DrawFramebuffer(albedo, Rectangle{view.viewport.x, view.viewport.y, (float)w, (float)h}, tint);
    ctx.PopShader();
    ctx.PopBlendMode();

    ctx.PushScissorMode((int)view.viewport.x, (int)view.viewport.y, (int)view.viewport.width, (int)view.viewport.height);
}

// RenderSystem — glue: owns the camera list and script draw-command queue, drives
// RenderSorter -> RenderCompositor per camera per frame.

RenderSystem::RenderSystem(Context context) : System(context) {
}

RenderSystem::~RenderSystem() {
}

SystemParameters RenderSystem::DefaultParameters() const {
    return {
        {"hierarchySort", Value{true}},
        {"ySort", Value{true}},
    };
}

void RenderSystem::OnParametersChanged() {
    _sorter.SetSortOptions({
        .hierarchySort = GetParameter("hierarchySort", true),
        .ySort = GetParameter("ySort", true),
    });
}

void RenderSystem::IssueDrawCommand(DrawCommand cmd) {
    _drawCommands.push_back(std::move(cmd));
}

void RenderSystem::Draw() {
    ProfileN("RenderSystem Draw");

    FindCameras();
    CollectOccluders();
    RenderContext ctx(*services, *world);

    if (services->Get<Services::IApplicationService>().GetMode() == AppMode::Editor) {
        // Editor mode renders through the free editor camera, not any in-scene CameraComponent,
        // across the whole framebuffer, which is sized to the viewport panel.
        auto& editorService = services->Get<Services::IEditorService>();
        const Framebuffer& target = services->Get<Services::ISceneService>().GetFramebuffer();
        auto& editorCam = editorService.GetEditorCamera();

        CameraView view{
            editorCam.position,
            editorCam.zoom != 0.0f ? editorCam.zoom : 1.0f,
            Rectangle{0, 0, (float)target.Width(), (float)target.Height()}
        };

        PlaceScreenInWorld(view);
        RenderView(ctx, view);
    } else {
        // Only play mode needs an in-scene camera; the editor (and prefab documents, which
        // never have one) always renders through the editor camera above.
        if (_cameraEntities.empty()) {
            ClearBackground(::BLACK);
            DrawText("No active camera found", 10, 10, 20, ::RED);
        }
        for (auto& cameraEntity : _cameraEntities) {
            auto& camera = world->GetComponent<CameraComponent>(cameraEntity);
            if (!camera.isVisible) continue;
            RenderView(ctx, MakeCameraView(cameraEntity));
        }
    }

    _compositor.PruneEntityBuffers();
    _drawCommands.clear();
}

void RenderSystem::CollectOccluders() {
    ProfileN("Collect Occluders");
    std::vector<RenderCompositor::ShadowCaster> casters;
    std::unordered_map<Entity, RenderCompositor::GroundLine> groundLines;
    world->Query<TransformComponent, OccluderComponent>([&](Entity entity, auto&, auto& occluder) {
        OccluderVolume volume = ResolveOccluder(*world, entity);
        const auto& footprint = volume.footprint;
        if (footprint.size() < 3) return;

        // The ground line runs through the footprint's centre along its long axis (the
        // principal axis of its vertices), when it clearly has one: a wall, not a pillar.
        Vector2 centre{0.0f, 0.0f};
        for (const Vector2& p : footprint) centre = {centre.x + p.x, centre.y + p.y};
        centre = {centre.x / footprint.size(), centre.y / footprint.size()};
        float xx = 0.0f, xy = 0.0f, yy = 0.0f;
        for (const Vector2& p : footprint) {
            const float dx = p.x - centre.x, dy = p.y - centre.y;
            xx += dx * dx; xy += dx * dy; yy += dy * dy;
        }
        const float spread = std::sqrt((xx - yy) * (xx - yy) * 0.25f + xy * xy);
        const float major = (xx + yy) * 0.5f + spread, minor = (xx + yy) * 0.5f - spread;
        float slope = 0.0f;
        if (major > minor * 6.0f) {
            const float angle = 0.5f * std::atan2(2.0f * xy, xx - yy);
            slope = std::clamp(std::tan(angle), -1.0f, 1.0f);  // a wall running up the screen can't lean further
        }
        groundLines[entity] = {centre, slope, volume.height <= 0.0f && occluder.isStatic};

        if (occluder.castsShadow && volume.height > 0.0f) casters.push_back({footprint, volume.height, entity});
    });
    _compositor.SetOccluders(std::move(casters), std::move(groundLines));

    // Lights stand `height` above their entity's position, as sprites stand on theirs.
    // Flicker is a few incommensurate sines per light, in brightness and a little in
    // position, so shadows breathe with it.
    std::vector<RenderCompositor::PointLight> lights;
    const float time = (float)::GetTime();
    // A hidden light (its entity or an ancestor invisible, or lost in the fog) lights nothing.
    // It fades rather than switching, over kLightFadeSeconds, so its pool doesn't pop as units
    // move and it drifts in and out of sight.
    constexpr float kLightFadeSeconds = 0.35f;
    const double now = ::GetTime();
    const float fadeStep = (float)std::clamp(now - lightFadeTime_, 0.0, 0.25) / kLightFadeSeconds;
    lightFadeTime_ = now;
    std::unordered_map<Entity, float> fades;
    auto hidden = [&](Entity e) {
        for (int depth = 0; e != INVALID_ENTITY && depth < 64; ++depth) {
            if (world->HasComponent<LayerComponent>(e)) {
                const auto& layer = world->GetComponent<LayerComponent>(e);
                if (!layer.isVisible || layer.inFog) return true;
            }
            e = world->HasComponent<ParentComponent>(e) ? world->GetComponent<ParentComponent>(e).parent : INVALID_ENTITY;
        }
        return false;
    };
    world->Query<TransformComponent, LightComponent>([&](Entity entity, auto& transform, auto& light) {
        auto previous = lightFade_.find(entity);
        float fade = previous == lightFade_.end() ? (hidden(entity) ? 0.0f : 1.0f) : previous->second;
        fade = std::clamp(fade + (hidden(entity) ? -fadeStep : fadeStep), 0.0f, 1.0f);
        fades[entity] = fade;
        if (fade <= 0.0f) return;
        const float seed = (float)entity * 1.618f;
        float brightness = light.intensity * fade * fade * (3.0f - 2.0f * fade);  // smoothstep
        Vector2 ground{transform.worldX, transform.worldY};
        if (light.flicker > 0.0f) {
            const float wave = 0.5f * std::sin(time * 7.3f + seed) + 0.3f * std::sin(time * 13.1f + seed * 2.0f) +
                               0.2f * std::sin(time * 23.7f + seed * 3.0f);
            brightness *= 1.0f + 0.35f * light.flicker * wave;
            ground.x += 1.5f * light.flicker * std::sin(time * 9.1f + seed * 4.0f);
            ground.y += 1.0f * light.flicker * std::sin(time * 8.3f + seed * 5.0f);
        }
        lights.push_back({WorldTo3D(ground, light.height),
                          Vector3{light.color.r / 255.0f * brightness, light.color.g / 255.0f * brightness,
                                  light.color.b / 255.0f * brightness},
                          light.radius, light.vision, entity});
    });
    lightFade_ = std::move(fades);
    _compositor.SetLights(std::move(lights));
}

void RenderSystem::PlaceScreenInWorld(CameraView& view) {
    // The game screen is the game resolution, not the editor view's.
    const auto& config = services->Get<Services::IApplicationService>().GetConfig();
    view.screenInWorld = true;
    view.screenScale = 1.0f;
    view.screenOrigin = { -config.framebufferWidth * 0.5f, -config.framebufferHeight * 0.5f };

    // Lowest renderOrder, the same camera play mode draws first.
    Entity camera = INVALID_ENTITY;
    const CameraComponent* best = nullptr;
    world->Query<CameraComponent>([&](Entity entity, const CameraComponent& component) {
        if (!best || component.renderOrder < best->renderOrder) { camera = entity; best = &component; }
    });
    if (camera == INVALID_ENTITY) return;

    CameraView game = MakeCameraView(camera);
    // Screen pixels are framebuffer pixels; the game camera shows world position
    // FramebufferToWorld(p) at pixel p, so that is where the screen goes.
    view.screenScale = 1.0f / game.zoom;
    view.screenOrigin = RenderProjector::FramebufferToWorld({0.0f, 0.0f}, game);
}

void RenderSystem::FindCameras() {
    ProfileN("RenderSystem FindCameras");
    _cameraEntities.clear();

    world->Query<CameraComponent>([&](Entity entity, auto&) {
        _cameraEntities.push_back(entity);
    });

    std::sort(_cameraEntities.begin(), _cameraEntities.end(), [&](Entity a, Entity b) {
        auto& camA = world->GetComponent<CameraComponent>(a);
        auto& camB = world->GetComponent<CameraComponent>(b);
        return camA.renderOrder < camB.renderOrder;
    });
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

void RenderSystem::RenderView(RenderContext& ctx, const CameraView& view) {
    ProfileN("RenderSystem RenderView");

    ctx.PushScissorMode(
        (int)view.viewport.x,
        (int)view.viewport.y,
        (int)view.viewport.width,
        (int)view.viewport.height);

    _sorter.Build(*world, *scene, _drawCommands, view);

    const auto& layers = _sorter.GetLayers();
    const auto& queue = _sorter.GetQueue();

    size_t queueStart = 0;
    for (size_t li = 0; li < layers.size(); li++) {
        const SceneLayer& layer = layers[li];
        if (!layer.isVisible) {
            while (queueStart < queue.size() && queue[queueStart].layerIndex == (uint8_t)li)
                queueStart++;
            continue;
        }

        size_t sliceStart = queueStart;
        while (queueStart < queue.size() && queue[queueStart].layerIndex == (uint8_t)li)
            queueStart++;
        size_t sliceEnd = queueStart;

        if (sliceStart == sliceEnd) continue;

        std::span<const RenderRecord> slice(queue.data() + sliceStart, sliceEnd - sliceStart);
        _compositor.RenderLayer(ctx, view, layer, slice);
    }

    ctx.PopScissorMode();
}

std::vector<Entity> RenderSystem::Pick(Vector2 fbPos, Entity cameraEntity) {
    if (!world->HasComponent<CameraComponent>(cameraEntity) ||
        !world->HasComponent<TransformComponent>(cameraEntity)) {
        return {};
    }
    return Pick(fbPos, MakeCameraView(cameraEntity));
}

// Pick and GetEntityRenderInfo read the queue built by the last Draw. Since then the
// editor may have removed the component a record came from (or the whole entity), and its
// Pick/Bounds would read a component that is no longer there.
static bool IsStale(const World& world, const RenderRecord& rec, const RenderableType& type) {
    return type.Has && !type.Has(world, rec.entity);
}

std::vector<Entity> RenderSystem::Pick(Vector2 fbPos, const CameraView& view) {
    std::vector<Entity> hits;
    Vector2 worldPos = RenderProjector::FramebufferToWorld(fbPos, view);
    Vector2 screenPos = RenderProjector::FramebufferToScreen(fbPos, view);

    const auto& queue = _sorter.GetQueue();
    auto& registry = RenderableRegistry::Instance();

    // queue is sorted in paint order; walking it back-to-front yields topmost-drawn-first.
    for (auto it = queue.rbegin(); it != queue.rend(); ++it) {
        const RenderRecord& rec = *it;
        const RenderableType& type = registry.Get(rec.typeId);
        if (!type.Pick || IsStale(*world, rec, type)) continue;

        Vector2 testPos = rec.isWorldSpace ? worldPos : screenPos;
        if (type.Pick(*world, rec, testPos)) {
            // Records for the same entity are adjacent in the sort — dedup via last-pushed.
            if (hits.empty() || hits.back() != rec.entity) hits.push_back(rec.entity);
        }
    }

    return hits;
}

EntityRenderInfo RenderSystem::GetEntityRenderInfo(Entity entity) {
    EntityRenderInfo info;
    auto& registry = RenderableRegistry::Instance();
    bool foundAny = false;
    for (const auto& rec : _sorter.GetQueue()) {
        if (rec.entity != entity || IsStale(*world, rec, registry.Get(rec.typeId))) continue;
        // isWorldSpace is per-entity (set once from its layer in CollectEntities), identical
        // across every record the entity produced — safe to take from the first match.
        if (!foundAny) {
            info.isWorldSpace = rec.isWorldSpace;
            foundAny = true;
        }
        if (!info.bounds) {
            const RenderableType& type = registry.Get(rec.typeId);
            if (type.Bounds) info.bounds = type.Bounds(*world, rec);
        }
    }
    return info;
}

// Analytic descriptions for the SDF material path (MaterialComponent). Each mirrors the
// placement its Render function uses, so swapping a shape onto materials doesn't move it.

static bool GeometryRectangleImpl(const World& world, const RenderRecord& rec, SdfGeometry& out) {
    const auto& component = world.GetComponent<RectangleComponent>(rec.entity);
    out.box = LocalBoundsRectangle(world, rec);
    // cornerRadius is raylib "roundness": a 0..1 fraction of half the shorter side.
    out.cornerRadius = component.cornerRadius * std::min(component.width, component.height) * 0.5f;
    out.geometry = out.cornerRadius > 0.0f ? "RoundedRect" : "Rect";
    return true;
}

static bool GeometryCircleImpl(const World& world, const RenderRecord& rec, SdfGeometry& out) {
    out.geometry = "Circle";
    out.box = LocalBoundsCircle(world, rec);
    return true;
}

static bool GeometryEllipseImpl(const World& world, const RenderRecord& rec, SdfGeometry& out) {
    out.geometry = "Ellipse";
    out.box = LocalBoundsEllipse(world, rec);
    return true;
}

static bool GeometryLineImpl(const World& world, const RenderRecord& rec, SdfGeometry& out) {
    const auto& component = world.GetComponent<LineComponent>(rec.entity);
    out.geometry = "Segment";
    out.box = LocalBoundsLine(world, rec);
    // Relative to the box center, which is the segment's midpoint.
    Vector2 center = { (component.x1 + component.x2) * 0.5f, (component.y1 + component.y2) * 0.5f };
    out.pointA = { component.x1 - center.x, component.y1 - center.y };
    out.pointB = { component.x2 - center.x, component.y2 - center.y };
    out.thickness = component.thickness;
    return true;
}

static bool GeometryPolygonImpl(const World& world, const RenderRecord& rec, SdfGeometry& out) {
    const auto& component = world.GetComponent<PolygonComponent>(rec.entity);
    const size_t count = component.points.size();
    if (count < 3 || count > (size_t)kMaxSdfPolygonPoints) return false;  // not drawable

    const Rectangle local = BoundingBox(component.points);
    Vector2 center = { local.x + local.width * 0.5f, local.y + local.height * 0.5f };

    out.geometry = "Polygon";
    out.box = Rectangle{ rec.x + local.x, rec.y + local.y, local.width, local.height };
    out.points.reserve(count);
    for (const Vector2& point : component.points) out.points.push_back({ point.x - center.x, point.y - center.y });
    return true;
}

// Grouped here rather than in each component's .cpp — avoids a Components->Systems include.
REGISTER_RENDERABLE(HasTileImpl,      RenderTileImpl,      nullptr,           nullptr,             false, nullptr)
REGISTER_RENDERABLE(HasRectangleImpl, nullptr,             PickRectangleImpl, BoundsRectangleImpl, false, GeometryRectangleImpl)
REGISTER_RENDERABLE(HasCircleImpl,    nullptr,             PickCircleImpl,    BoundsCircleImpl,    false, GeometryCircleImpl)
REGISTER_RENDERABLE(HasTextImpl,      RenderTextImpl,      PickTextImpl,      nullptr,             false, nullptr)
REGISTER_RENDERABLE(HasEllipseImpl,   nullptr,             PickEllipseImpl,   BoundsEllipseImpl,   false, GeometryEllipseImpl)
REGISTER_RENDERABLE(HasLineImpl,      nullptr,             PickLineImpl,      BoundsLineImpl,      false, GeometryLineImpl)
REGISTER_RENDERABLE(HasPolygonImpl,   nullptr,             PickPolygonImpl,   BoundsPolygonImpl,   false, GeometryPolygonImpl)
REGISTER_RENDERABLE(HasShaderImpl,    RenderShaderImpl,    PickShaderImpl,    BoundsShaderImpl,    false, nullptr)

namespace {
bool RegisterDrawCommandRenderables() {
    auto& reg = RenderableRegistry::Instance();
    g_circleCmdTypeId  = reg.Register(RenderableType{ nullptr, RenderDrawCircleCmd,  nullptr, nullptr, false });
    g_lineCmdTypeId    = reg.Register(RenderableType{ nullptr, RenderDrawLineCmd,    nullptr, nullptr, false });
    g_rectCmdTypeId    = reg.Register(RenderableType{ nullptr, RenderDrawRectCmd,    nullptr, nullptr, false });
    g_ellipseCmdTypeId = reg.Register(RenderableType{ nullptr, RenderDrawEllipseCmd, nullptr, nullptr, false });
    g_textCmdTypeId    = reg.Register(RenderableType{ nullptr, RenderDrawTextCmd,    nullptr, nullptr, false });
    g_polygonCmdTypeId = reg.Register(RenderableType{ nullptr, RenderDrawPolygonCmd, nullptr, nullptr, false });
    return true;
}
bool _registered_draw_command_renderables = RegisterDrawCommandRenderables();
}  // namespace

}  // namespace Elysium::Systems

REGISTER_SYSTEM(Elysium::Systems::RenderSystem)
