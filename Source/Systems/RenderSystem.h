#pragma once

#include "Core/System.h"
#include "Components/CameraComponent.h"
#include "Core/Scene.h"
#include "Core/Renderable.h"
#include "Core/Graphics.h"
#include "Core/Framebuffer.h"
#include "Core/ShadowAtlas.h"
#include "Core/World3D.h"
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <variant>
#include "Core/RenderContext.h"

namespace Elysium::Services { class IAssetService; }
namespace Elysium { class Path; }

namespace Elysium::Systems {

// A camera's render-projection parameters, decoupled from any CameraComponent entity —
// lets the editor's free camera drive the same render/pick/project math as a real camera.
struct CameraView {
    Vector2 position;
    float zoom;
    Rectangle viewport;

    // Editor only: where Screen2D layers sit in the world, so they pan/zoom with the
    // editor camera like everything else. Screen pixel p is drawn at world
    // screenOrigin + p * screenScale. Unset (in play), Screen2D layers are drawn 1:1 in
    // framebuffer pixels.
    bool screenInWorld = false;
    Vector2 screenOrigin{};
    float screenScale = 1.0f;

    // Editor only: the orbit around `position` (see World3D::View). The default is the camera
    // the ground picture is drawn from; turned away from it, ground layers lie on the ground plane.
    float yaw = 0.0f;
    float pitch = World3D::kDefaultPitch;

    bool IsDefaultOrientation() const { return yaw == 0.0f && pitch == World3D::kDefaultPitch; }
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
    // A Screen2D position <-> framebuffer; the identity unless view.screenInWorld.
    static Vector2 ScreenToFramebuffer(Vector2 screenPos, const CameraView& view);
    static Vector2 FramebufferToScreen(Vector2 fbPos, const CameraView& view);
    // The 3D view the camera looks through. World positions above are ground positions.
    static World3D::View View3D(const CameraView& view);
};

// Builds one frame's sorted RenderRecord queue for a single CameraView: culls + collects
// ECS entities (via RenderableRegistry) and script-issued draw commands into one queue.
class RenderSorter {
public:
    // Which keys order records within a layer; RenderSystem's parameters of the same names.
    struct SortOptions {
        bool hierarchySort = true;  // parents before children, siblings in child order
    };
    void SetSortOptions(const SortOptions& options) { sortOptions_ = options; }

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
    std::unordered_set<std::string> hiddenLayerOverride_;
    bool unlitOverride_ = false;
    uint32_t nextCollectionOrder_ = 0;
    SortOptions sortOptions_;

public:
    // Editor only: layers to treat as invisible this frame, on top of each SceneLayer's own
    // isVisible. Applied to the sorter's private copy of the layer list, so the editor's
    // hide/solo never becomes scene data. Empty in play mode.
    void SetHiddenLayerOverride(std::unordered_set<std::string> layers) { hiddenLayerOverride_ = std::move(layers); }
    // Editor only: every layer drawn unlit this frame (the viewport's lighting switch), on the
    // same private copy.
    void SetUnlitOverride(bool unlit) { unlitOverride_ = unlit; }
};

// Renders one layer's slice of a RenderSorter's queue: a World3D layer in 3D (Render3D), or a
// ground or screen layer flat, immediate-mode straight to the framebuffer or composited through
// an offscreen buffer (blend modes, opacity, ambient).
class RenderCompositor {
public:
    // A LightComponent, already placed in the 3D world (see WorldTo3D in RenderSystem.cpp)
    // and flickered for this frame. color is premultiplied by intensity.
    struct PointLight {
        Vector3 position;
        Vector3 color;
        float radius = 1.0f;
        bool vision = false;            // also clears the fog of war where it can see
        Entity owner = INVALID_ENTITY;
    };
    void SetLights(std::vector<PointLight> lights) { lights_ = std::move(lights); }

    void RenderLayer(RenderContext& ctx, const CameraView& view,
                      const SceneLayer& layer, std::span<const RenderRecord> records);

    // Drops the retained offscreen buffer of every entity that wasn't shaded this frame —
    // destroyed entities, unloaded scenes, or just a ShaderComponent switched off. Called
    // once per frame at the end of RenderSystem::Draw.
    void PruneEntityBuffers();

private:
    void RenderImmediate(RenderContext& ctx, const CameraView& view,
                          const SceneLayer& layer, std::span<const RenderRecord> records);
    void RenderComposited(RenderContext& ctx, const CameraView& view,
                           const SceneLayer& layer, std::span<const RenderRecord> records);
    // A World3D layer, through the iso camera with a depth buffer (Core/World3D.h): first its
    // models, then everything else as upright cards standing at their root's ground position,
    // far to near, depth-tested against the models but not each other.
    void Render3D(RenderContext& ctx, const CameraView& view,
                  const SceneLayer& layer, std::span<const RenderRecord> records);
    // Walks the layer's records in order, grouping the contiguous run each entity produced
    // so a ShaderComponent entity can be diverted through RenderShadedEntity as a unit.
    // `enclosingTarget` is the framebuffer already bound by the caller. raylib's
    // EndTextureMode unconditionally drops to the backbuffer, so a shaded entity's detour
    // has to be told what to re-bind afterwards.
    void RenderRecords(RenderContext& ctx, std::span<const RenderRecord> records,
                       const Matrix& layerTransform, const Framebuffer& enclosingTarget);

    // An entity's records: through its materials if it has an enabled MaterialComponent,
    // otherwise each record's own Render.
    void RenderEntity(RenderContext& ctx, Entity entity, std::span<const RenderRecord> records);

    // Draws one entity's own records (through its materials, if it has any) into a private offscreen buffer sized to its bounds
    // plus ShaderComponent::padding, then blits that buffer back into the layer through the
    // entity's shader — so the shader sees exactly the entity's pixels in texture0, with
    // room around them for glow/outline effects to bleed into.
    // `projection`: re-applied before the blit (World3D cards, whose depth the default
    // ortho would clip), with depth testing back on.
    void RenderShadedEntity(RenderContext& ctx, Entity entity, std::span<const RenderRecord> records,
                            const Matrix& layerTransform, const Framebuffer& enclosingTarget,
                            const Matrix* projection = nullptr);

    // Analytic path: each record whose renderable reports SdfGeometry is drawn as one quad
    // per enabled MaterialLayer (in order), through the shader composed from that
    // geometry + the layer's material. Records without geometry render normally.
    void RenderMaterialEntity(RenderContext& ctx, Entity entity, std::span<const RenderRecord> records);

    Shader* GetComposedShader(Services::IAssetService& assets, const char* geometry, const std::string& material);
    Shader* GetShader(Services::IAssetService& assets, const Path& path);
    static void PushBlend(RenderContext& ctx, SceneLayerBlend blend);
    const Framebuffer& EnsureCompositeBuffer(int width, int height);
    const Framebuffer& EnsureEntityBuffer(Entity entity, int width, int height);

    Framebuffer compositeBuffer_;
    // This frame's point lights, shared by every World3D layer.
    std::vector<PointLight> lights_;
    // One retained buffer per shaded entity, resized when its bounds change. Kept across
    // frames because allocating a render texture per entity per frame is not viable.
    std::unordered_map<Entity, Framebuffer> entityBuffers_;
    std::unordered_set<Entity> shadedThisFrame_;
    std::unordered_set<std::string> requestedShaders_;
    std::unordered_set<std::string> requestedModels_;
    // World3D: how faded each model is (1 solid), easing toward faded while it stands between
    // the camera and a unit (MovementComponent).
    std::unordered_map<Entity, float> modelFade_;
    double modelFadeTime_ = 0.0;
    // World3D shadows: the point lights' cube maps of every model, and each model's GL-space
    // triangles, rebuilt only when its matrix or model changes.
    ShadowAtlas shadowAtlas3D_{16};
    struct ModelTriangles {
        Matrix matrix;
        const void* model = nullptr;
        std::vector<Vector3> triangles;
        Vector3 min, max;  // the triangles' bounds
    };
    std::unordered_map<Entity, ModelTriangles> modelTriangles_;
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

    // Fills the view's screen placement for the editor: the game screen is laid over the
    // first camera's view rect (the red box the editor draws), or centered on the world
    // origin at 1:1 when the scene has no camera.
    void PlaceScreenInWorld(CameraView& view);

    // Editor only: layers the layer drawer is hiding or soloing away. See the sorter's method.
    void SetHiddenLayerOverride(std::unordered_set<std::string> layers) {
        _sorter.SetHiddenLayerOverride(std::move(layers));
    }
    // Editor only: the viewport's lighting switch. See the sorter's method.
    void SetUnlitOverride(bool unlit) { _sorter.SetUnlitOverride(unlit); }

protected:
    SystemParameters DefaultParameters() const override;
    void OnParametersChanged() override;

private:
    void FindCameras();
    // Hands the compositor this frame's point lights.
    void CollectLights();
    CameraView MakeCameraView(Entity cameraEntity);
    void RenderView(RenderContext& ctx, const CameraView& view);

    std::vector<Entity> _cameraEntities;
    std::vector<DrawCommand> _drawCommands;
    RenderSorter _sorter;
    RenderCompositor _compositor;
    // Per light, how far it has faded in (1) or out (0) as it comes into sight or goes into
    // the fog, so it eases instead of popping.
    std::unordered_map<Entity, float> lightFade_;
    double lightFadeTime_ = 0.0;
};

}  // namespace Elysium::Systems
