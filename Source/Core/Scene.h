#pragma once

#include "System.h"
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "Entity.h"
#include "Event.h"
#include "Message.h"
#include "Core/Xml.h"
namespace Elysium {

class ServiceLocator;

enum class SceneLayerSpace {
    // The world, in 3D (Core/World3D.h): models with depth, and everything else as upright
    // cards facing the camera, or lying on the ground (SceneLayer::ground).
    World3D,
    // The game screen, in its own pixels: UI.
    Screen2D,
};

enum class SceneLayerBlend {
    Normal,
    Additive,
    Multiply,
};

struct SceneLayer {
    std::string name;
    int zIndex = 0;                 // Render order among layers — lower draws first (see RenderSorter)
    bool isVisible = true;
    bool isComposited = false;      // If composited, the layer will render to a render target instead of immediately to the framebuffer
    float opacity = 1.0f;

    SceneLayerSpace space = SceneLayerSpace::World3D;
    // World3D: what isn't a model lies flat on the ground (selection rings, shadow blobs, move
    // markers) instead of standing up as a card. Its position is a point in the ground picture,
    // as the default camera sees it (see Core/World3D.h), drawn in layer order, without depth.
    bool ground = false;
    SceneLayerBlend layerBlend = SceneLayerBlend::Normal;      // how objects in this layer are blended when rendered with respect to each other
    SceneLayerBlend compositeBlend = SceneLayerBlend::Normal;  // how this layer is blended when composited onto the framebuffer

    Color ambient{0, 0, 0, 0};  // composited layers: what the layer's buffer is cleared to

    // Lighting, for World3D layers' models and cards (RenderCompositor::Render3D): the ambient,
    // plus every LightComponent in reach, shadowed by the models.
    Color lightAmbient{96, 96, 112, 255};   // light everywhere, before any light
    // A sun: light from one direction everywhere (no shadows of its own). Black is off.
    Color sunColor{0, 0, 0, 255};
    float sunIntensity = 1.0f;
    float sunYaw = -45.0f;    // degrees around the vertical it shines from (0: from the camera's side)
    float sunPitch = 50.0f;   // degrees above the horizon
    // Models: a bright edge where a surface turns away from the camera, so characters stand
    // out of dark ground. 0 off.
    float rimLight = 0.0f;
    bool shadows = true;                    // the models cast shadows
    float shadowBias = 12.0f;               // world units a point is lifted off its surface before the shadow test
    // How far what no vision light (LightComponent::vision) can see fades to fogColor. 0 off,
    // 1 hidden.
    float fogOfWar = 0.0f;
    Color fogColor{6, 6, 12, 255};
    // The editor's lighting switch is off (not saved): drawn as if fully lit, with no lights,
    // shadows or fog. Set on the render sorter's copy only, see RenderSorter.
    bool unlit = false;

    // Whether this layer is lit (a World3D layer standing in the world, not lying on the ground).
    bool IsLit() const { return space == SceneLayerSpace::World3D && !ground; }

    static constexpr const char* XmlTag() { return "SceneLayer"; }

    static void LoadXml(SceneLayer& layer, tinyxml2::XMLElement* el);
};

struct SceneConfiguration {
    std::string name;

    float resolutionWidth = 640.0f;
    float resolutionHeight = 480.0f;

    std::vector<SceneLayer> layers;
};

class Scene final : public IEventListener, IMessageListener {
   public:
    explicit Scene(ServiceLocator& services);
    virtual ~Scene();

    ServiceLocator& GetServices() const { return services_; }

    // Hook methods - can be overridden by subclasses
    virtual void OnUpdate(float deltaTime, bool isPlaying);
    virtual void OnDraw(Rectangle screen);
    virtual void OnEvent(Event& event) override;
    virtual void OnMessage(Message& message) override;
    virtual void OnEnter() {}
    virtual void OnExit() {}

    // Entity and system access
    World* GetWorld() { return world_.get(); }
    const std::vector<std::unique_ptr<System>>& GetSystems() const { return systems_; }
    
    template <typename T>
    T* GetSystem() const {
        for (const auto& system : systems_) {
            if (T* t = dynamic_cast<T*>(system.get())) {
                return t;
            }
        }
        return nullptr;
    }

    std::vector<SceneLayer>& GetLayers();
    const std::vector<SceneLayer>& GetLayers() const;
    SceneLayer* GetLayer(const std::string& name);
    const SceneLayer* GetLayer(const std::string& name) const;
    void AddLayer(const SceneLayer& layer);

    const SceneConfiguration& GetConfiguration() const { return configuration_; }
    void SetConfiguration(const SceneConfiguration& config) { configuration_ = config; }

    void AddSystem(std::unique_ptr<System> system);
    void RemoveSystem(System* system);

    // Takes `host`'s configuration and layers, and fresh copies of its systems (only the
    // ones that run while paused, if `pausedSystemsOnly`). Lets a scratch scene (a prefab
    // document, prefab defaults being settled) render and settle like the host would.
    void CopySetupFrom(const Scene& host, bool pausedSystemsOnly);

    // Called during XML loading to create scene-specific systems
    virtual void CreateCustomSystems() {}

    // What the framebuffer is cleared to behind this scene; unset uses the window's
    // configured background. The editor sets it on its documents to match its theme.
    void SetBackgroundColor(std::optional<Color> color) { backgroundColor_ = color; }
    const std::optional<Color>& GetBackgroundColor() const { return backgroundColor_; }

    // The scene's name is its file's stem (Scenes/<name>.xml); both are set by LoadScene.
    const std::string& GetName() const { return name_; }
    const std::string& GetPath() const { return path_; }
    void SetSource(const std::string& name, const std::string& path) { name_ = name; path_ = path; }

    // Attributes the editor keeps with the scene (its grid), loaded and saved verbatim from
    // <EditorMetadata>. The engine never reads them.
    std::map<std::string, std::string>& GetEditorMetadata() { return editorMetadata_; }
    const std::map<std::string, std::string>& GetEditorMetadata() const { return editorMetadata_; }

    void SetSceneScript(const std::string& path) { sceneScriptPath_ = path; }
    const std::string& GetSceneScript() const { return sceneScriptPath_; }

protected:
    // Core scene components
    ServiceLocator& services_;
    std::unique_ptr<World> world_;
    std::vector<std::unique_ptr<System>> systems_;
    SceneConfiguration configuration_;
    std::vector<SceneLayer> layers_;
    std::string sceneScriptPath_;
    std::string name_, path_;
    std::map<std::string, std::string> editorMetadata_;
    std::optional<Color> backgroundColor_;
    bool isSceneScriptInitialized_ = false;
};

// The file of the scene called `name`: Scenes/<name>.xml under the project.
std::string ScenePath(const std::string& name);

bool LoadScene(Scene& scene, const std::string& path);
bool SaveScene(Scene& scene, const std::string& path);

// Loads every child component of one <Entity> onto `entity` through the ComponentRegistry,
// including the CameraComponent -> FollowComponent/ParentComponent special case.
void LoadEntityComponents(tinyxml2::XMLElement* xmlEntity, World* world, Entity entity, ServiceLocator& services);

}  // namespace Elysium
