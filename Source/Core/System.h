#pragma once
#include <string>

#include "Core/Event.h"
#include "Core/Message.h"

namespace Elysium {
class Application;
class Scene;
class World;
}  // namespace Elysium

namespace Elysium {

struct Context {
    Application* application;
    Scene* scene;
    World* world;
};

class System : public IEventListener, public IMessageListener {
protected:
    Application* application;
    Scene* scene;
    World* world;

    bool isEnabled_ = true;
    bool isVisible_ = true;
    std::string name_;

public:
    System(Context context) : application(context.application), scene(context.scene), world(context.world) {
    }
    virtual ~System() = default;

    virtual void Update(float deltaTime) {}
    virtual void Draw() {}
    
    virtual void OnEvent(Event& event) override {}
    virtual void OnMessage(Message& message) override {}

    // Name is set by SystemRegistry::Create() from the registered key.
    // Returns empty string if the system was not created through the registry.
    const std::string& GetName() const { return name_; }
    void SetName(const std::string& name) { name_ = name; }

    Scene* GetScene() const { return scene; }

    bool IsEnabled() const { return isEnabled_; }
    void SetEnabled(bool enabled) { isEnabled_ = enabled; }

    bool IsVisible() const { return isVisible_; }
    void SetVisible(bool visible) { isVisible_ = visible; }
};

}  // namespace Elysium
