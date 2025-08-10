#pragma once

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

    virtual void Update(float deltaTime) = 0;
    virtual void Render()
    {
    }
    virtual void OnEvent(const char *eventName, void *data)
    {
    }
};
} // namespace Elysium
