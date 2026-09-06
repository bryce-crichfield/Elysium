#pragma once

#include <string>
#include <vector>

#include "Core/Path.h"
#include "Core/Event.h"
#include "Core/Entity.h"
#include "Core/Script.h"
#include "Interfaces/IService.h"

namespace Elysium {
class World;
}

namespace Elysium::Services {

class IScriptService : public IService {
   public:

    virtual Elysium::ScriptResult ExecuteString(const std::string& scriptString) = 0;

    virtual std::vector<Entity> FilterEntities(const std::string& filterFunctionBody) = 0;

    virtual bool InitializeScene(Path scriptPath) = 0;
    virtual bool UpdateScene(Path scriptPath, float deltaTime) = 0;
    virtual bool RenderScene(Path scriptPath) = 0;
    virtual void OnSceneEvent(Path scriptPath, Event& event) = 0;

    virtual bool InitializeEntity(Entity entity, Path scriptPath) = 0;
    virtual bool UpdateEntity(Entity entity, Path scriptPath, float deltaTime) = 0;
    virtual void OnEntityEvent(Entity entity, Path scriptPath, Event& event) = 0;

    virtual void ReloadScript(Path scriptPath) = 0;

    virtual void InspectEntityScript(Entity entity, Path scriptPath) = 0;

    virtual void SetActiveWorld(World* w) = 0;

    virtual void SetMousePosition(float x, float y) = 0;
};

}  // namespace Elysium::Services
