#pragma once

#include <cstddef>
#include <string>
#include <memory>
#include <vector>
#include <functional>
#include "Entity.h"
#include "Asset.h"

namespace Elysium {

struct Application;

struct InputEvent {
    enum Type {
        KEY_PRESS,
        KEY_RELEASE,
        MOUSE_PRESS,
        MOUSE_RELEASE,
        MOUSE_MOVE
    };
    
    Type type;
    int key;
    float x, y;
};

struct NetworkEvent {
    enum Type {
        CLIENT_CONNECTED,
        CLIENT_DISCONNECTED,
        DATA_RECEIVED,
        CONNECTION_ERROR
    };
    
    Type type;
    int clientId;
    void* data;
    size_t dataSize;
};

class Scene {
public:
    Scene(const std::string& name);
    virtual ~Scene() = default;
    
    const std::string& GetName() const { return name_; }
    
    // Load scene from XML file
    virtual void LoadFromXML(const std::string& xmlPath);
    
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
    virtual void OnNetwork(const NetworkEvent& event) {}
    virtual void OnInput(const InputEvent& event) {}

    virtual void OnEnter() {}
    virtual void OnExit() {}
    
    // Entity and system access
    EntityWorld* GetWorld() { return world_.get(); }
    const std::vector<std::unique_ptr<System>>& GetSystems() const { return systems_; }

protected:
    // Called during XML loading to create scene-specific systems
    virtual void CreateCustomSystems() {}
    
    std::string name_;
    
    // Core scene components
    std::unique_ptr<EntityWorld> world_;
    std::vector<std::unique_ptr<System>> systems_;
};

// Scene factory function type - declared after Scene class is defined
using SceneFactory = std::function<std::unique_ptr<Scene>()>;

} // namespace Elysium