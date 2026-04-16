#pragma once

#include "Core/System.h"
#include "Components/CameraComponent.h"
#include "Components/RectangleComponent.h"
#include "Components/CircleComponent.h"
#include "Components/TextComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/TileComponent.h"
#include "Components/LightComponent.h"
#include "Core/SceneLayer.h"
#include "raylib.h"
#include <span>
#include <vector>
#include <string>
#include <variant>
#include "Core/RenderContext.h"

namespace Elysium::Systems {

// Bitmask of renderable components an entity has — set during collect, used during render
enum RenderComponentFlags : uint8_t {
    RC_Rectangle = 1 << 0,
    RC_Circle    = 1 << 1,
    RC_Text      = 1 << 2,
    RC_Sprite    = 1 << 3,
    RC_Light     = 1 << 4,
    RC_Tile      = 1 << 5,
};

// Lightweight sort key — no component data, just enough to sort and identify the entity
struct RenderKey {
    uint8_t  layerIndex;
    uint8_t  hierarchyDepth;
    uint8_t  componentMask;
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

class RenderSystem : public System {
public:
    using CameraEntity = std::pair<Entity, CameraComponent>;

    RenderSystem(Context context);
    ~RenderSystem();

    static RenderSystem* GetCurrent();
    void IssueDrawCommand(DrawCommand cmd);

    void Draw() override;

protected:
    virtual void RenderView(RenderContext& ctx, Entity cameraEntity);
    virtual void RenderImmediateLayer(RenderContext& ctx, Entity cameraEntity, const SceneLayer& layer, std::span<const RenderKey> keys);
    virtual void RenderCompositedLayer(RenderContext& ctx, Entity cameraEntity, const SceneLayer& layer, std::span<const RenderKey> keys);

    // Per-entity dispatch — calls per-component methods based on componentMask
    void RenderEntity(RenderContext& ctx, const RenderKey& key, const SceneLayer& layer);

    // Per-component render methods — fetch directly from ECS, no copies
    void RenderRectangle(RenderContext& ctx, Entity entity, Vector2 pos, const SceneLayer& layer);
    void RenderCircle   (RenderContext& ctx, Entity entity, Vector2 pos, const SceneLayer& layer);
    void RenderText     (RenderContext& ctx, Entity entity, Vector2 pos, const SceneLayer& layer);
    void RenderSprite   (RenderContext& ctx, Entity entity, Vector2 pos, const SceneLayer& layer);
    void RenderTile     (RenderContext& ctx, Entity entity, Vector2 pos, const SceneLayer& layer);
    void RenderLight    (RenderContext& ctx, Entity entity, Vector2 pos, const SceneLayer& layer);

    // Render deferred draw commands for a given layer
    void RenderDrawCommands(RenderContext& ctx, const SceneLayer& layer);

    void FindCameras();
    std::string GetLayerName(Entity entity);

    void PushBlendMode(RenderContext& ctx, const SceneLayerBlend& blend);
    Matrix CalculateTransform(Entity camera, const SceneLayer& layer);

    RenderTexture2D& EnsureLightMap(int width, int height);

    std::vector<Entity>      _cameraEntities;
    std::vector<DrawCommand> _drawCommands;
    std::vector<RenderKey>   _renderQueue;    // Reused each frame — reserved once, never deallocated

    RenderTexture2D _lightMap = {0};
    int _lightMapWidth  = 0;
    int _lightMapHeight = 0;

    bool _isIsometric       = false;
    bool _isIsometricCached = false;
};

}  // namespace Elysium::Systems