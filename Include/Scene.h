#pragma once

#include <cstddef>
#include <string>
#include <memory>
#include <vector>
#include <functional>
#include "Entity.h"

namespace Elysium {

struct GameConfig;

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
    Scene(const std::string& name, const GameConfig& config);
    virtual ~Scene() = default;
    
    const std::string& GetName() const { return name_; }
    
    // Load scene from XML file
    virtual void LoadFromXML(const std::string& xmlPath);
    
    // Hook methods - can be overridden by subclasses
    virtual void OnUpdate(float deltaTime);
    virtual void OnDraw();
    virtual void OnDebugDraw() {}
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
    const GameConfig& config_;
    
    // Core scene components
    std::unique_ptr<EntityWorld> world_;
    std::vector<std::unique_ptr<System>> systems_;
};

// Scene factory function type - declared after Scene class is defined
using SceneFactory = std::function<std::unique_ptr<Scene>(const GameConfig&)>;

} // namespace Elysium