#pragma once

#include "Core/System.h"
#include "Components/CameraComponent.h"
#include "Components/RectangleComponent.h"
#include "Components/CircleComponent.h"
#include "Components/TextComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/LightComponent.h"
#include "Components/DebugComponent.h"
#include "Core/SceneLayer.h"
#include "raylib.h"
#include <vector>
#include <map>
#include <variant>
#include <optional>
#include <string>
#include "Core/RenderContext.h"

namespace Elysium::Systems {

using Renderable = std::variant<
    RectangleComponent,
    CircleComponent,
    TextComponent,
    SpriteComponent,
    LightComponent,
    DebugComponent
>;

struct RenderableObject {
    Entity entity;
    Vector2 position;
    Renderable renderable;
};

using RenderableObjects = std::vector<RenderableObject>;

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
    virtual void RenderImmediateLayer(RenderContext& ctx, Entity cameraEntity, const SceneLayer& layer, const std::vector<RenderableObject>& objects);
    virtual void RenderCompositedLayer(RenderContext& ctx, Entity cameraEntity, const SceneLayer& layer, const std::vector<RenderableObject>& objects);
    virtual void RenderObject(RenderContext& ctx, const RenderableObject& object, const SceneLayer& layer);

    void FindCameras();
    std::vector<Renderable> GetRenderables(Entity entity);
    std::string GetLayerName(Entity entity);

    void PushBlendMode(RenderContext& ctx, const SceneLayerBlend& blend);
    Matrix CalculateTransform(Entity camera, const SceneLayer& layer);

    // Ensure light map is sized correctly, returns the texture
    RenderTexture2D& EnsureLightMap(int width, int height);

    std::vector<Entity> _cameraEntities;
    std::vector<DrawCommand> _drawCommands;
    RenderTexture2D _lightMap = {0};  // Cached light map render target
    int _lightMapWidth = 0;
    int _lightMapHeight = 0;
};

}  // namespace Elysium::Systems
