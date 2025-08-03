#pragma once

#include <cstddef>

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

class IScene {
public:
    virtual ~IScene() = default;
    
    virtual void OnUpdate(float deltaTime) = 0;
    virtual void OnDraw() = 0;
    virtual void OnNetwork(const NetworkEvent& event) = 0;
    virtual void OnInput(const InputEvent& event) = 0;
    
    virtual void OnEnter() {}
    virtual void OnExit() {}
};