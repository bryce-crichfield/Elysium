#pragma once
#include <string>
#include <typeinfo>

namespace Elysium {
class Application;
class Scene;
class World;
}

namespace Elysium
{

struct Context
{
    Application *application;
    Scene *scene;
    World *world;
};

class System
{
  protected:
    Application *application;
    Scene *scene;
    World *world;

  public:
    System(Context context) : application(context.application), scene(context.scene), world(context.world)
    {
    }
    virtual ~System() = default;

    virtual void Update(float deltaTime) {}
    virtual void Draw() {}
    std::string GetName() const { return typeid(*this).name(); }
};

} // namespace Elysium
