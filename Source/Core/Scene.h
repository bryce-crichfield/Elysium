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
#include "Core/SceneLayer.h"
namespace Elysium {

// Constants
constexpr float TILE_WIDTH = 32.0f;
constexpr float TILE_HEIGHT = 32.0f;

struct Application;

struct SceneConfiguration {
    std::string name;

    float resolutionWidth = 640.0f;
    float resolutionHeight = 480.0f;

    std::vector<SceneLayer> layers;
};

class Scene : public IEventListener, IMessageListener {
   public:
    Scene();
    virtual ~Scene();

    // Hook methods - can be overridden by subclasses
    virtual void OnUpdate(float deltaTime);
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

    // Called during XML loading to create scene-specific systems
    virtual void CreateCustomSystems() {}

protected:
    // Core scene components
    std::unique_ptr<World> world_;
    std::vector<std::unique_ptr<System>> systems_;
    SceneConfiguration configuration_;
    std::vector<SceneLayer> layers_;
};

// Scene factory function type - declared after Scene class is defined
using SceneFactory = std::function<Scene*()>;

bool LoadScene(Scene& scene, const std::string& path);
bool SaveScene(Scene& scene, const std::string& path);

}  // namespace Elysium
