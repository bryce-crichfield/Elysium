#pragma once

#include "Core/System.h"
#include "Components/CameraComponent.h"
#include "Components/RectangleComponent.h"
#include "Components/CircleComponent.h"
#include "Components/EllipseComponent.h"
#include "Components/LineComponent.h"
#include "Components/PolygonComponent.h"
#include "Components/TextComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/TextureComponent.h"
#include "Components/TileComponent.h"
#include "Components/LightComponent.h"
#include "Core/SceneLayer.h"
#include "raylib.h"
#include <span>
#include <unordered_set>
#include <vector>
#include <string>
#include <variant>
#include "Core/RenderContext.h"

namespace Elysium::Systems {

// Bitmask of renderable components an entity has — set during collect, used during render
enum RenderComponentFlags : uint16_t {
    RC_Rectangle = 1 << 0,
    RC_Circle    = 1 << 1,
    RC_Text      = 1 << 2,
    RC_Sprite    = 1 << 3,
    RC_Light     = 1 << 4,
    RC_Tile      = 1 << 5,
    RC_Ellipse   = 1 << 6,
    RC_Line      = 1 << 7,
    RC_Polygon   = 1 << 8,
};

// Lightweight sort key — no component data, just enough to sort and identify the entity
struct RenderKey {
    uint8_t  layerIndex;
    uint8_t  hierarchyDepth;
    uint8_t  childIndex;
    uint16_t componentMask;
    uint8_t  isWorldSpace;   // 1 = World2D (Y-sort); 0 = Screen2D (declaration order)
    float    y;
    float    x;
    Entity   entity;
};

// Deferred draw commands issued by Lua scripts
struct DrawCircleCmd  { std::string layer; float x, y, radius; Color color; };
struct DrawEllipseCmd { std::string layer; float x, y, radiusH, radiusV; Color color; };
struct DrawLineCmd    { std::string layer; float x1, y1, x2, y2; Color color; };
struct DrawRectCmd    { std::string layer; float x, y, width, height; Color color; };
struct DrawTextCmd    { std::string layer; std::string text; float x, y; int fontSize; Color color; };
struct DrawPolygonCmd { std::string layer; std::vector<Vector2> points; Color color; };

using DrawCommand = std::variant<DrawCircleCmd, DrawLineCmd, DrawRectCmd, DrawEllipseCmd, DrawTextCmd, DrawPolygonCmd>;

// A camera's render-projection parameters, decoupled from any CameraComponent entity — lets
// the editor's free camera (Services::EditorCamera) drive the same render/pick/project math
// as a real in-scene camera without needing an ECS entity of its own.
struct CameraView {
    Vector2 position;
    float zoom;
    Rectangle viewport;
};

class RenderSystem : public System {
public:
    using CameraEntity = std::pair<Entity, CameraComponent>;

    RenderSystem(Context context);
    ~RenderSystem();

    static RenderSystem* GetCurrent();
    void IssueDrawCommand(DrawCommand cmd);

    void Draw() override;

    // Returns entities under the given framebuffer-space point, ordered topmost-drawn-first.
    // Reflects the render queue as of the last Draw() call (one frame stale at worst — a
    // non-issue while the simulation is paused, since nothing moves between frames then).
    std::vector<Entity> Pick(Vector2 fbPos, Entity cameraEntity);
    std::vector<Entity> Pick(Vector2 fbPos, const CameraView& view);

    // Forward of Pick()'s screen->world math — projects a World2D point to framebuffer
    // space, e.g. so editor gizmo handles know where to draw/hit-test themselves.
    Vector2 WorldToFramebuffer(Vector2 worldPos, Entity cameraEntity);
    Vector2 WorldToFramebuffer(Vector2 worldPos, const CameraView& view);

    // Shared between DrawSelectionOverlay's handle drawing and ViewportEditor's hit-testing
    // so the visual size and the click tolerance can't drift apart.
    static constexpr float kMoveHandleRadius = 8.0f;

protected:
    bool PickRectangle(Entity entity, Vector2 testPos, Vector2 pos, bool isWorldSpace);
    bool PickCircle   (Entity entity, Vector2 testPos, Vector2 pos);
    bool PickEllipse  (Entity entity, Vector2 testPos, Vector2 pos);
    bool PickSprite   (Entity entity, Vector2 testPos, Vector2 pos);
    bool PickText     (Entity entity, Vector2 testPos, Vector2 pos, bool isWorldSpace);
    bool PickLight    (Entity entity, Vector2 testPos, Vector2 pos);
    bool PickPolygon  (Entity entity, Vector2 testPos, Vector2 pos);

    // Builds a CameraView from a real camera entity's CameraComponent + TransformComponent.
    CameraView MakeCameraView(Entity cameraEntity);

    virtual void RenderView(RenderContext& ctx, const CameraView& view);
    virtual void RenderImmediateLayer(RenderContext& ctx, const CameraView& view, const SceneLayer& layer, std::span<const RenderKey> keys);
    virtual void RenderCompositedLayer(RenderContext& ctx, const CameraView& view, const SceneLayer& layer, std::span<const RenderKey> keys);

    // Per-entity dispatch — calls per-component methods based on componentMask
    void RenderEntity(RenderContext& ctx, const RenderKey& key, const SceneLayer& layer);

    // Per-component render methods — fetch directly from ECS, no copies
    void RenderRectangle(RenderContext& ctx, Entity entity, Vector2 pos, const SceneLayer& layer);
    void RenderCircle   (RenderContext& ctx, Entity entity, Vector2 pos, const SceneLayer& layer);
    void RenderText     (RenderContext& ctx, Entity entity, Vector2 pos, const SceneLayer& layer);
    void RenderSprite   (RenderContext& ctx, Entity entity, Vector2 pos, const SceneLayer& layer);
    void RenderTile     (RenderContext& ctx, Entity entity, Vector2 pos, const SceneLayer& layer);
    void RenderLight    (RenderContext& ctx, Entity entity, Vector2 pos, const SceneLayer& layer);
    void RenderEllipse  (RenderContext& ctx, Entity entity, Vector2 pos, const SceneLayer& layer);
    void RenderLine     (RenderContext& ctx, Entity entity, Vector2 pos, const SceneLayer& layer);
    void RenderPolygon  (RenderContext& ctx, Entity entity, Vector2 pos, const SceneLayer& layer);

    // Render deferred draw commands for a given layer
    void RenderDrawCommands(RenderContext& ctx, const SceneLayer& layer);

    void FindCameras();
    std::string GetLayerName(Entity entity);

    // Rebuilds _hiddenEntities each frame: seeds from entities whose own
    // LayerComponent::isVisible is false (e.g. VisibilitySystem's fog-of-war
    // flag), then cascades that hidden state down through World::GetAllChildren
    // so descendants without their own LayerComponent inherit it too.
    void ComputeHiddenEntities();

    // Editor-mode-only highlight drawn around EditorService's currently selected entities.
    void DrawSelectionOverlay(RenderContext& ctx, const CameraView& view);

    // Editor-mode-only: draws each real CameraComponent's viewport bounds as a colored
    // rectangle in the editor camera's view, since it no longer renders through them directly.
    void DrawCameraGizmos(RenderContext& ctx, const CameraView& editorView);

    void DrawAxes(RenderContext& ctx, const CameraView& editorView);

    void PushBlendMode(RenderContext& ctx, const SceneLayerBlend& blend);
    Matrix CalculateTransform(const CameraView& view, const SceneLayer& layer);

    RenderTexture2D& EnsureLightMap(int width, int height);

    std::vector<Entity>      _cameraEntities;
    std::vector<DrawCommand> _drawCommands;
    std::vector<RenderKey>   _renderQueue;    // Reused each frame — reserved once, never deallocated
    std::unordered_set<Entity> _hiddenEntities;  // Rebuilt each frame by ComputeHiddenEntities

    RenderTexture2D _lightMap = {0};
    int _lightMapWidth  = 0;
    int _lightMapHeight = 0;

    bool _isIsometric       = false;
    bool _isIsometricCached = false;

    RenderSystem* _previousRenderSystem = nullptr;
};

}  // namespace Elysium::Systems