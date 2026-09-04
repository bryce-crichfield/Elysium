#include "Systems/RenderSystem.h"
#include <algorithm>
#include <cmath>
#include <type_traits>
#include "Components/CameraComponent.h"
#include "Components/CircleComponent.h"
#include "Components/EllipseComponent.h"
#include "Components/LayerComponent.h"
#include "Components/LightComponent.h"
#include "Components/LineComponent.h"
#include "Components/ParentComponent.h"
#include "Components/PolygonComponent.h"
#include "Components/RectangleComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/TextComponent.h"
#include "Components/TextureComponent.h"
#include "Components/TileComponent.h"
#include "Components/TransformComponent.h"
#include "Core/Common.h"
#include "Core/Entity.h"
#include "Core/Geometry.h"
#include "Core/Path.h"
#include "Core/RenderContext.h"
#include "Core/Scene.h"
#include "Core/ServiceLocator.h"
#include "Core/SystemRegistry.h"
#include "Core/Tile.h"
#include "Interfaces/IApplicationService.h"
#include "Interfaces/IAssetService.h"
#include "Interfaces/IEditorService.h"
#include "Interfaces/ISceneService.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

namespace Elysium::Systems {

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
static bool HasSpriteImpl(const World& world, Entity entity)    { return world.HasComponent<TextureComponent>(entity); }
static bool HasTextImpl(const World& world, Entity entity)      { return world.HasComponent<TextComponent>(entity); }
static bool HasLightImpl(const World& world, Entity entity)     { return world.HasComponent<LightComponent>(entity); }
static bool HasEllipseImpl(const World& world, Entity entity)   { return world.HasComponent<EllipseComponent>(entity); }
static bool HasLineImpl(const World& world, Entity entity)      { return world.HasComponent<LineComponent>(entity); }
static bool HasPolygonImpl(const World& world, Entity entity)   { return world.HasComponent<PolygonComponent>(entity); }

static void RenderTileImpl(RenderContext& ctx, const RenderRecord& rec) {
    const auto& comp = ctx.GetWorld().GetComponent<TileComponent>(rec.entity);
    auto& assets = ctx.GetServices().Get<Elysium::Services::IAssetService>();

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
        rec.x - frameWidth  * tile.originX,
        rec.y - frameHeight * tile.originY,
        frameWidth,
        frameHeight
    };

    ctx.DrawTexturePro(texture, sourceRect, destRect, {0, 0}, 0.0f, comp.tint);
}

static void RenderRectangleImpl(RenderContext& ctx, const RenderRecord& rec) {
    const auto& component = ctx.GetWorld().GetComponent<RectangleComponent>(rec.entity);

    float topLeftX = rec.isWorldSpace ? rec.x - component.width  * 0.5f : rec.x;
    float topLeftY = rec.isWorldSpace ? rec.y - component.height * 0.5f : rec.y;

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
        auto& assets = ctx.GetServices().Get<Elysium::Services::IAssetService>();
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

static bool PickRectangleImpl(const World& world, const RenderRecord& rec, Vector2 testPos) {
    const auto& component = world.GetComponent<RectangleComponent>(rec.entity);
    float left = rec.isWorldSpace ? rec.x - component.width  * 0.5f : rec.x;
    float top  = rec.isWorldSpace ? rec.y - component.height * 0.5f : rec.y;
    return testPos.x >= left && testPos.x <= left + component.width &&
           testPos.y >= top  && testPos.y <= top + component.height;
}

static std::optional<Rectangle> BoundsRectangleImpl(const World& world, const RenderRecord& rec) {
    const auto& component = world.GetComponent<RectangleComponent>(rec.entity);
    float left = rec.isWorldSpace ? rec.x - component.width  * 0.5f : rec.x;
    float top  = rec.isWorldSpace ? rec.y - component.height * 0.5f : rec.y;
    return Rectangle{ left, top, component.width, component.height };
}

static void RenderCircleImpl(RenderContext& ctx, const RenderRecord& rec) {
    const auto& component = ctx.GetWorld().GetComponent<CircleComponent>(rec.entity);
    ctx.DrawCircle(rec.x, rec.y, component.radius, component.background);
    if (component.border.a > 0) {
        ctx.DrawCircleLines(rec.x, rec.y, component.radius, component.border);
    }
}

static bool PickCircleImpl(const World& world, const RenderRecord& rec, Vector2 testPos) {
    const auto& component = world.GetComponent<CircleComponent>(rec.entity);
    return Vector2Distance(testPos, {rec.x, rec.y}) <= component.radius;
}

static std::optional<Rectangle> BoundsCircleImpl(const World& world, const RenderRecord& rec) {
    const auto& component = world.GetComponent<CircleComponent>(rec.entity);
    return Rectangle{ rec.x - component.radius, rec.y - component.radius, component.radius * 2.0f, component.radius * 2.0f };
}

static void RenderEllipseImpl(RenderContext& ctx, const RenderRecord& rec) {
    const auto& component = ctx.GetWorld().GetComponent<EllipseComponent>(rec.entity);
    ctx.DrawEllipse(rec.x, rec.y, component.radiusH, component.radiusV, component.background);
    if (component.border.a > 0) {
        ctx.DrawEllipseLines(rec.x, rec.y, component.radiusH, component.radiusV, component.border);
    }
}

static bool PickEllipseImpl(const World& world, const RenderRecord& rec, Vector2 testPos) {
    const auto& component = world.GetComponent<EllipseComponent>(rec.entity);
    if (component.radiusH <= 0.0f || component.radiusV <= 0.0f) return false;
    float dx = (testPos.x - rec.x) / component.radiusH;
    float dy = (testPos.y - rec.y) / component.radiusV;
    return (dx * dx + dy * dy) <= 1.0f;
}

static void RenderLineImpl(RenderContext& ctx, const RenderRecord& rec) {
    const auto& component = ctx.GetWorld().GetComponent<LineComponent>(rec.entity);
    ctx.DrawLineEx(rec.x + component.x1, rec.y + component.y1,
                   rec.x + component.x2, rec.y + component.y2,
                   component.thickness, component.color);
}

static void RenderPolygonImpl(RenderContext& ctx, const RenderRecord& rec) {
    const auto& component = ctx.GetWorld().GetComponent<PolygonComponent>(rec.entity);
    if (component.points.size() < 3) return;

    std::vector<Vector2> worldPoints;
    worldPoints.reserve(component.points.size());
    for (const auto& p : component.points) {
        worldPoints.push_back({ rec.x + p.x, rec.y + p.y });
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

static bool PickPolygonImpl(const World& world, const RenderRecord& rec, Vector2 testPos) {
    const auto& component = world.GetComponent<PolygonComponent>(rec.entity);
    if (component.points.size() < 3) return false;

    std::vector<Vector2> worldPoints;
    worldPoints.reserve(component.points.size());
    for (const auto& p : component.points) {
        worldPoints.push_back({ rec.x + p.x, rec.y + p.y });
    }
    return PointInPolygon(testPos, worldPoints);
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

static void RenderSpriteImpl(RenderContext& ctx, const RenderRecord& rec) {
    const World& world = ctx.GetWorld();
    if (!world.HasComponent<TextureComponent>(rec.entity)) return;
    const auto& tex = world.GetComponent<TextureComponent>(rec.entity);
    if (tex.textureName.empty()) return;

    auto& assets = ctx.GetServices().Get<Elysium::Services::IAssetService>();
    Texture2D texture = assets.GetTexture(Path(tex.textureName));
    if (texture.id == 0) return;

    float scaleX = 1.0f, scaleY = 1.0f, rotation = 0.0f;
    if (world.HasComponent<TransformComponent>(rec.entity)) {
        const auto& transform = world.GetComponent<TransformComponent>(rec.entity);
        scaleX = transform.worldScaleX;
        scaleY = transform.worldScaleY;
        rotation = transform.worldRotation;
    }

    float scaledWidth  = tex.sourceRect.width  * scaleX;
    float scaledHeight = tex.sourceRect.height * scaleY;

    // DrawTexturePro ignores destRect's sign, so negative scale (mirroring) is expressed via
    // a negated source-rect width instead, plus a 180deg rotation for vertical-only flips.
    bool flipHorizontal = scaledWidth  < 0.0f;
    bool flipVertical   = scaledHeight < 0.0f;

    Rectangle sourceRect = tex.sourceRect;
    if (flipHorizontal != flipVertical) sourceRect.width = -sourceRect.width;
    float drawRotation = rotation + (flipVertical ? 180.0f : 0.0f);

    float absWidth  = std::fabs(scaledWidth);
    float absHeight = std::fabs(scaledHeight);

    Vector2 origin = { absWidth * tex.originX, absHeight * tex.originY };
    Rectangle destRect = { rec.x, rec.y, absWidth, absHeight };

    SetTextureFilter(texture, tex.filterMode == TextureFilterMode::Bilinear ? TEXTURE_FILTER_BILINEAR : TEXTURE_FILTER_POINT);
    ctx.DrawTexturePro(texture, sourceRect, destRect, origin, drawRotation, tex.tint);
}

static bool PickSpriteImpl(const World& world, const RenderRecord& rec, Vector2 testPos) {
    if (!world.HasComponent<TextureComponent>(rec.entity)) return false;
    const auto& tex = world.GetComponent<TextureComponent>(rec.entity);

    float scaleX = 1.0f, scaleY = 1.0f, rotation = 0.0f;
    if (world.HasComponent<TransformComponent>(rec.entity)) {
        const auto& transform = world.GetComponent<TransformComponent>(rec.entity);
        scaleX = transform.worldScaleX;
        scaleY = transform.worldScaleY;
        rotation = transform.worldRotation;
    }

    float scaledWidth  = tex.sourceRect.width  * scaleX;
    float scaledHeight = tex.sourceRect.height * scaleY;

    bool flipVertical = scaledHeight < 0.0f;
    float drawRotation = rotation + (flipVertical ? 180.0f : 0.0f);

    float absWidth  = std::fabs(scaledWidth);
    float absHeight = std::fabs(scaledHeight);
    Vector2 origin = { absWidth * tex.originX, absHeight * tex.originY };

    // Undo DrawTexturePro's rotation to land back in the sprite's local space before testing.
    float rad = -drawRotation * DEG2RAD;
    float dx = testPos.x - rec.x;
    float dy = testPos.y - rec.y;
    float localX = dx * cosf(rad) - dy * sinf(rad) + origin.x;
    float localY = dx * sinf(rad) + dy * cosf(rad) + origin.y;

    return localX >= 0.0f && localX <= absWidth && localY >= 0.0f && localY <= absHeight;
}

static std::optional<Rectangle> BoundsSpriteImpl(const World& world, const RenderRecord& rec) {
    if (!world.HasComponent<TextureComponent>(rec.entity)) return std::nullopt;
    const auto& tex = world.GetComponent<TextureComponent>(rec.entity);

    float scaleX = 1.0f, scaleY = 1.0f;
    if (world.HasComponent<TransformComponent>(rec.entity)) {
        const auto& transform = world.GetComponent<TransformComponent>(rec.entity);
        scaleX = transform.worldScaleX;
        scaleY = transform.worldScaleY;
    }

    float absWidth  = std::fabs(tex.sourceRect.width  * scaleX);
    float absHeight = std::fabs(tex.sourceRect.height * scaleY);
    return Rectangle{ rec.x - absWidth * tex.originX, rec.y - absHeight * tex.originY, absWidth, absHeight };
}

static void RenderLightImpl(RenderContext& ctx, const RenderRecord& rec) {
    const auto& component = ctx.GetWorld().GetComponent<LightComponent>(rec.entity);
    const int numRings = 8;

    for (int i = 0; i < numRings; i++) {
        float t     = (float)i / numRings;
        float power = 1.0f + component.intensity * 4.0f;
        float curve = powf(1.0f - t, power);
        float ringRadius = component.radius * curve;

        Color ringColor = component.color;
        ringColor.a = (unsigned char)(component.color.a / numRings);
        Color ringEdge  = { ringColor.r, ringColor.g, ringColor.b, 0 };

        if (ctx.IsIsometric()) {
            ctx.DrawEllipseGradient(rec.x, rec.y, ringRadius, ringRadius * 0.5f, ringColor, ringEdge);
        } else {
            ctx.DrawCircleGradient(rec.x, rec.y, ringRadius, ringColor, ringEdge);
        }
    }
}

static bool PickLightImpl(const World& world, const RenderRecord& rec, Vector2 testPos) {
    const auto& component = world.GetComponent<LightComponent>(rec.entity);
    return Vector2Distance(testPos, {rec.x, rec.y}) <= component.radius;
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
            // MatrixMultiply(A, B) applies A first — must match the WorldToFramebuffer math below.
            return MatrixMultiply(MatrixMultiply(cameraTranslation, scale), centerTranslation);
        }
    }
    return MatrixIdentity();
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

void RenderSorter::ComputeHiddenEntities(World& world) {
    ProfileN("RenderSorter ComputeHiddenEntities");
    hiddenEntities_.clear();

    // Seed with entities hidden via their own LayerComponent, then cascade down the
    // hierarchy so children without their own LayerComponent inherit it too.
    std::vector<Entity> stack;
    world.Query<LayerComponent>([&](Entity entity, const LayerComponent& layerComp) {
        if (!layerComp.isVisible) stack.push_back(entity);
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

        if (!skipCull && (pos.x < worldLeft || pos.x > worldRight ||
                           pos.y < worldTop  || pos.y > worldBottom))
            return;

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
    std::sort(queue_.begin(), queue_.end(), [](const RenderRecord& a, const RenderRecord& b) {
        if (a.layerIndex != b.layerIndex) return a.layerIndex < b.layerIndex;
        if (a.hierarchyDepth != b.hierarchyDepth) return a.hierarchyDepth < b.hierarchyDepth;
        if (a.childIndex != b.childIndex) return a.childIndex < b.childIndex;
        // Y-sort only for World2D layers; ties (and Screen2D layers) fall back to collection order.
        if (a.isWorldSpace && a.y != b.y) return a.y < b.y;
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

RenderCompositor::~RenderCompositor() {
    if (compositeBuffer_.id != 0) {
        UnloadRenderTexture(compositeBuffer_);
    }
}

RenderTexture2D& RenderCompositor::EnsureCompositeBuffer(int width, int height) {
    if (compositeBuffer_.id == 0 || compositeWidth_ != width || compositeHeight_ != height) {
        if (compositeBuffer_.id != 0) {
            UnloadRenderTexture(compositeBuffer_);
        }
        compositeBuffer_ = LoadRenderTexture(width, height);
        compositeWidth_ = width;
        compositeHeight_ = height;
    }
    return compositeBuffer_;
}

void RenderCompositor::PushBlend(RenderContext& ctx, SceneLayerBlend blend) {
    switch (blend) {
        case SceneLayerBlend::Additive: ctx.PushBlendMode(BLEND_ADDITIVE);   break;
        case SceneLayerBlend::Multiply: ctx.PushBlendMode(BLEND_MULTIPLIED); break;
        default:                        ctx.PushBlendMode(BLEND_ALPHA);     break;
    }
}

void RenderCompositor::RenderRecords(RenderContext& ctx, std::span<const RenderRecord> records) {
    ProfileN("Render Records");
    auto& registry = RenderableRegistry::Instance();
    for (const auto& rec : records) {
        const RenderableType& type = registry.Get(rec.typeId);
        if (type.Render) type.Render(ctx, rec);
    }
}

void RenderCompositor::RenderLayer(RenderContext& ctx, const CameraView& view,
                                    const SceneLayer& layer, std::span<const RenderRecord> records) {
    if (layer.isComposited) {
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

    RenderRecords(ctx, records);

    ctx.PopMatrix();
    ctx.PopBlendMode();
}

void RenderCompositor::RenderComposited(RenderContext& ctx, const CameraView& view,
                                         const SceneLayer& layer, std::span<const RenderRecord> records) {
    ProfileN("Render Composited Layer");

    Matrix layerTransform = RenderProjector::CalculateTransform(view, layer);

    int w = (int)view.viewport.width;
    int h = (int)view.viewport.height;
    RenderTexture2D& compositionBuffer = EnsureCompositeBuffer(w, h);

    ctx.PopScissorMode();

    ctx.BeginTextureMode(compositionBuffer);
    ClearBackground(layer.ambient);

    PushBlend(ctx, layer.layerBlend);
    ctx.PushMatrix();
    ctx.MultiplyMatrix(layerTransform);

    RenderRecords(ctx, records);

    ctx.PopMatrix();
    ctx.PopBlendMode();

    ctx.EndTextureMode();

    // Restore SceneService's framebuffer
    auto& sceneService = ctx.GetServices().Get<Services::ISceneService>();
    ctx.BeginTextureMode(sceneService.GetFramebuffer());

    PushBlend(ctx, layer.compositeBlend);
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

// RenderSystem — glue: owns the camera list and script draw-command queue, drives
// RenderSorter -> RenderCompositor per camera per frame.

RenderSystem::RenderSystem(Context context) : System(context) {
}

RenderSystem::~RenderSystem() {
}

void RenderSystem::IssueDrawCommand(DrawCommand cmd) {
    _drawCommands.push_back(std::move(cmd));
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
    RenderContext ctx(*services, *world, _isIsometric);

    if (services->Get<Services::IApplicationService>().GetMode() == AppMode::Editor) {
        // Editor mode renders through the free editor camera, not any in-scene CameraComponent.
        auto& editorService = services->Get<Services::IEditorService>();
        const auto& config = services->Get<Services::IApplicationService>().GetConfig();
        auto& editorCam = editorService.GetEditorCamera();

        CameraView view{
            editorCam.position,
            editorCam.zoom != 0.0f ? editorCam.zoom : 1.0f,
            Rectangle{0, 0, (float)config.framebufferWidth, (float)config.framebufferHeight}
        };

        RenderView(ctx, view);
    } else {
        for (auto& cameraEntity : _cameraEntities) {
            auto& camera = world->GetComponent<CameraComponent>(cameraEntity);
            if (!camera.isVisible) continue;
            RenderView(ctx, MakeCameraView(cameraEntity));
        }
    }

    _drawCommands.clear();
}

void RenderSystem::FindCameras() {
    ProfileN("RenderSystem FindCameras");
    _cameraEntities.clear();

    world->Query<CameraComponent>([&](Entity entity, auto&) {
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

std::vector<Entity> RenderSystem::Pick(Vector2 fbPos, const CameraView& view) {
    std::vector<Entity> hits;
    Vector2 worldPos = RenderProjector::FramebufferToWorld(fbPos, view);

    const auto& queue = _sorter.GetQueue();
    auto& registry = RenderableRegistry::Instance();

    // queue is sorted in paint order; walking it back-to-front yields topmost-drawn-first.
    for (auto it = queue.rbegin(); it != queue.rend(); ++it) {
        const RenderRecord& rec = *it;
        const RenderableType& type = registry.Get(rec.typeId);
        if (!type.Pick) continue;

        Vector2 testPos = rec.isWorldSpace ? worldPos : fbPos;
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
        if (rec.entity != entity) continue;
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

// Grouped here rather than in each component's .cpp — avoids a Components->Systems include.
REGISTER_RENDERABLE(HasTileImpl,      RenderTileImpl,      nullptr,           nullptr,             false)
REGISTER_RENDERABLE(HasRectangleImpl, RenderRectangleImpl, PickRectangleImpl, BoundsRectangleImpl, false)
REGISTER_RENDERABLE(HasCircleImpl,    RenderCircleImpl,    PickCircleImpl,    BoundsCircleImpl,    false)
REGISTER_RENDERABLE(HasSpriteImpl,    RenderSpriteImpl,    PickSpriteImpl,    BoundsSpriteImpl,    false)
REGISTER_RENDERABLE(HasTextImpl,      RenderTextImpl,      PickTextImpl,      nullptr,             false)
REGISTER_RENDERABLE(HasLightImpl,     RenderLightImpl,     PickLightImpl,     nullptr,             true)
REGISTER_RENDERABLE(HasEllipseImpl,   RenderEllipseImpl,   PickEllipseImpl,   nullptr,             false)
REGISTER_RENDERABLE(HasLineImpl,      RenderLineImpl,      nullptr,           nullptr,             false)
REGISTER_RENDERABLE(HasPolygonImpl,   RenderPolygonImpl,   PickPolygonImpl,   nullptr,             false)

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
