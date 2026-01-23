#pragma once
#include <string>
#include <typeinfo>

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

   public:
    System(Context context) : application(context.application), scene(context.scene), world(context.world) {
    }
    virtual ~System() = default;

    virtual void Update(float deltaTime) {}
    virtual void Draw() {}
    
    virtual void OnEvent(Event& event) override {}
    virtual void OnMessage(Message& message) override {}

    std::string GetName() const { return typeid(*this).name(); }
    Scene* GetScene() const { return scene; }
};

}  // namespace Elysium
