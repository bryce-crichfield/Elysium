#pragma once

#include <string>
#include <sol/sol.hpp>
#include "Core/Entity.h"
#include "Core/Event.h"
#include "Core/Path.h"

namespace Elysium {
class World;
}

namespace Elysium::Services {

class IScriptService {
   public:
    virtual ~IScriptService() = default;

    virtual sol::protected_function_result ExecuteString(const std::string& scriptString) = 0;

    virtual bool InitializeEntity(Entity entity, Path scriptPath) = 0;
    virtual bool UpdateEntity(Entity entity, Path scriptPath, float deltaTime) = 0;
    virtual void OnEntityEvent(Entity entity, Path scriptPath, Event& event) = 0;

    virtual bool InitializeScene(Path scriptPath) = 0;
    virtual bool UpdateScene(Path scriptPath, float deltaTime) = 0;
    virtual bool RenderScene(Path scriptPath) = 0;
    virtual void OnSceneEvent(Path scriptPath, Event& event) = 0;

    virtual void ReloadScript(Path scriptPath) = 0;

    virtual void InspectEntityScript(Entity entity, Path scriptPath) = 0;

    virtual sol::state& GetLua() = 0;

    virtual void SetActiveWorld(World* w) = 0;

    virtual void SetMousePosition(float x, float y) = 0;
};

}  // namespace Elysium::Services
