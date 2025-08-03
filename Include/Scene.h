#pragma once

#include <cstddef>
#include <string>

namespace Elysium {

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
    
    virtual void OnUpdate(float deltaTime) {}
    virtual void OnDraw() {}
    virtual void OnDebugDraw() {}
    virtual void OnNetwork(const NetworkEvent& event) {}
    virtual void OnInput(const InputEvent& event) {}
    
    virtual void OnEnter() {}
    virtual void OnExit() {}

protected:
    std::string name_;
};

} // namespace Elysium