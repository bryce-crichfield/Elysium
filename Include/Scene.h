#pragma once

namespace Elysium {
    class System; // This tells the compiler that the class System exists.
}

#include <cstddef>
#include <string>
#include <memory>
#include <vector>
#include <functional>
#include "Entity.h"
#include "Asset.h"

namespace Elysium {

// Constants
constexpr float TILE_WIDTH = 32.0f;
constexpr float TILE_HEIGHT = 32.0f;

struct Application;

class Scene {
public:
    Scene(const std::string& name);
    virtual ~Scene();

    const std::string& GetName() const { return name_; }

    // Load scene from XML file
    virtual void LoadFromXML(const std::string& xmlPath);

    // Set XML path for deferred loading
    void SetXmlPath(const std::string& xmlPath) { xmlPath_ = xmlPath; }

    // Called during loading, returns Asset objects with name, path, and type
    // application then passes these to loading service
    // application waits until loading service is done
    // loading service calls into asset service to perform the loading
    // application shows loading screen while processing assets
    virtual std::vector<Asset> GetAssets() { return {}; }

    // Hook methods - can be overridden by subclasses
    virtual void OnUpdate(float deltaTime);
    virtual void OnDraw(Rectangle screen);
    virtual void OnDebugDraw();

    // Add OnEventHandlers
    // virtual void OnNetwork(const NetworkEvent& event) {}
    // virtual void OnInput(const InputEvent& event) {}

    virtual void OnEnter() {
        // Load XML if path was set but not yet loaded
        if (!xmlPath_.empty()) {
            LoadFromXML(xmlPath_);
            xmlPath_.clear(); // Clear to prevent reloading
        }
    }
    virtual void OnExit() {}

    // Entity and system access
    World* GetWorld() { return world_.get(); }
    const std::vector<std::unique_ptr<System>>& GetSystems() const { return systems_; }
    void AddSystem(std::unique_ptr<System> system);

protected:
    // Called during XML loading to create scene-specific systems
    virtual void CreateCustomSystems() {}

    std::string name_;
    std::string xmlPath_;

    // Core scene components
    std::unique_ptr<World> world_;
    std::vector<std::unique_ptr<System>> systems_;
};

// Scene factory function type - declared after Scene class is defined
using SceneFactory = std::function<std::unique_ptr<Scene>()>;

} // namespace Elysium
