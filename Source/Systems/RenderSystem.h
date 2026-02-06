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
#include <vector>
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

class RenderSystem : public System {
public:
    using CameraEntity = std::pair<Entity, CameraComponent>;

    RenderSystem(Context context) : System(context) {}
    ~RenderSystem();

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
    RenderTexture2D _lightMap = {0};  // Cached light map render target
    int _lightMapWidth = 0;
    int _lightMapHeight = 0;
};

}  // namespace Elysium::Systems
