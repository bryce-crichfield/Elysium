#pragma once

#include "System.h"
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "Entity.h"
#include "Event.h"
#include "Message.h"
#include "Core/Xml.h"
namespace Elysium {

// Constants
constexpr float TILE_WIDTH = 32.0f;
constexpr float TILE_HEIGHT = 32.0f;

class ServiceLocator;

enum class SceneLayerSpace {
    World2D,
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

    SceneLayerSpace space = SceneLayerSpace::World2D;
    SceneLayerBlend layerBlend = SceneLayerBlend::Normal;      // how objects in this layer are blended when rendered with respect to each other
    SceneLayerBlend compositeBlend = SceneLayerBlend::Normal;  // how this layer is blended when composited onto the framebuffer

    Color ambient{0, 0, 0, 0};

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
    bool isSceneScriptInitialized_ = false;
};

// Scene factory function type - declared after Scene class is defined
using SceneFactory = std::function<Scene*(ServiceLocator&)>;

bool LoadScene(Scene& scene, const std::string& path);
bool SaveScene(Scene& scene, const std::string& path);

// Loads every child component of one <Entity> onto `entity` through the ComponentRegistry,
// including the CameraComponent -> FollowComponent/ParentComponent special case.
void LoadEntityComponents(tinyxml2::XMLElement* xmlEntity, World* world, Entity entity, ServiceLocator& services);

}  // namespace Elysium
