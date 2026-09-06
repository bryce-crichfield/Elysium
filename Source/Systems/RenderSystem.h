#pragma once

#include "Core/System.h"
#include "Components/CameraComponent.h"
#include "Core/Scene.h"
#include "Core/Renderable.h"
#include "raylib.h"
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <variant>
#include "Core/RenderContext.h"

namespace Elysium::Systems {

// A camera's render-projection parameters, decoupled from any CameraComponent entity —
// lets the editor's free camera drive the same render/pick/project math as a real camera.
struct CameraView {
    Vector2 position;
    float zoom;
    Rectangle viewport;
};

// Deferred draw commands issued by Lua scripts
struct DrawCircleCmd  { std::string layer; float x, y, radius; Color color; };
struct DrawEllipseCmd { std::string layer; float x, y, radiusH, radiusV; Color color; };
struct DrawLineCmd    { std::string layer; float x1, y1, x2, y2; Color color; };
struct DrawRectCmd    { std::string layer; float x, y, width, height; Color color; };
struct DrawTextCmd    { std::string layer; std::string text; float x, y; int fontSize; Color color; };
struct DrawPolygonCmd { std::string layer; std::vector<Vector2> points; Color color; };

using DrawCommand = std::variant<DrawCircleCmd, DrawLineCmd, DrawRectCmd, DrawEllipseCmd, DrawTextCmd, DrawPolygonCmd>;

// What ViewportEditor needs to draw/hit-test a selected entity's gizmo: whether its position
// is a world coordinate (needs CameraView projection) or already framebuffer/screen space (a
// Screen2D-layer entity, per RenderProjector::CalculateTransform's identity branch), plus its
// world-space bounds if its RenderableType registered a Bounds callback.
struct EntityRenderInfo {
    bool isWorldSpace = true;
    std::optional<Rectangle> bounds;
};

// Pure projection math — CameraView/SceneLayer -> screen space and back. The seam a future
// World3D CameraView variant would extend.
class RenderProjector {
public:
    static Matrix CalculateTransform(const CameraView& view, const SceneLayer& layer);
    static Vector2 WorldToFramebuffer(Vector2 worldPos, const CameraView& view);
    static Vector2 FramebufferToWorld(Vector2 fbPos, const CameraView& view);
};

// Builds one frame's sorted RenderRecord queue for a single CameraView: culls + collects
// ECS entities (via RenderableRegistry) and script-issued draw commands into one queue.
class RenderSorter {
public:
    void Build(World& world, Scene& scene, const std::vector<DrawCommand>& drawCommands, const CameraView& view);

    const std::vector<SceneLayer>& GetLayers() const { return layers_; }
    const std::vector<RenderRecord>& GetQueue() const { return queue_; }

private:
    void ComputeHiddenEntities(World& world);
    void CollectEntities(World& world, const CameraView& view);
    void CollectDrawCommands(const std::vector<DrawCommand>& drawCommands);
    void SortQueue();

    std::vector<SceneLayer> layers_;   // zIndex-sorted copy, rebuilt each frame
    std::unordered_map<std::string, uint8_t> layerNameToIndex_;
    uint8_t defaultLayerIndex_ = 0;
    std::vector<RenderRecord> queue_;
    std::unordered_set<Entity> hiddenEntities_;
    uint32_t nextCollectionOrder_ = 0;
};

// Renders one layer's slice of a RenderSorter's queue: immediate-mode straight to the
// framebuffer, or composited through an offscreen buffer (blend modes, opacity, ambient).
class RenderCompositor {
public:
    ~RenderCompositor();

    void RenderLayer(RenderContext& ctx, const CameraView& view,
                      const SceneLayer& layer, std::span<const RenderRecord> records);

private:
    void RenderImmediate(RenderContext& ctx, const CameraView& view,
                          const SceneLayer& layer, std::span<const RenderRecord> records);
    void RenderComposited(RenderContext& ctx, const CameraView& view,
                           const SceneLayer& layer, std::span<const RenderRecord> records);
    static void RenderRecords(RenderContext& ctx, std::span<const RenderRecord> records);
    static void PushBlend(RenderContext& ctx, SceneLayerBlend blend);
    RenderTexture2D& EnsureCompositeBuffer(int width, int height);

    RenderTexture2D compositeBuffer_ = {0};
    int compositeWidth_ = 0;
    int compositeHeight_ = 0;
};

class RenderSystem : public System {
public:
    RenderSystem(Context context);
    ~RenderSystem();

    void IssueDrawCommand(DrawCommand cmd);
    const std::vector<DrawCommand>& GetDrawCommands() const { return _drawCommands; }

    void Draw() override;

    // Ordered topmost-drawn-first. Reflects the queue as of the last Draw() call.
    std::vector<Entity> Pick(Vector2 fbPos, Entity cameraEntity);
    std::vector<Entity> Pick(Vector2 fbPos, const CameraView& view);

    // Reflects the queue as of the last Draw() call. Used by ViewportEditor for gizmo
    // drawing/hit-testing/drag math, since an entity's own layer determines whether its
    // position is world- or screen-space, not anything ViewportEditor can know on its own.
    EntityRenderInfo GetEntityRenderInfo(Entity entity);

private:
    void FindCameras();
    CameraView MakeCameraView(Entity cameraEntity);
    void RenderView(RenderContext& ctx, const CameraView& view);

    std::vector<Entity> _cameraEntities;
    std::vector<DrawCommand> _drawCommands;
    RenderSorter _sorter;
    RenderCompositor _compositor;

    bool _isIsometric = false;
    bool _isIsometricCached = false;
};

}  // namespace Elysium::Systems
