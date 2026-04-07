#pragma once

#include "Service.h"
#include <string>
#include <unordered_map>
#include "Core/Entity.h"
#include "Core/Event.h"
#include "Core/Path.h"
#include <sol/sol.hpp>

namespace Elysium::Services {

class ScriptService : public Elysium::Service {
public:
    ScriptService();
    ~ScriptService() override;

    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;

    // Core execution
    sol::protected_function_result ExecuteString(const std::string& scriptString);


    bool InitializeEntity(Entity entity, Path scriptPath);
    bool UpdateEntity(Entity entity, Path scriptPath, float deltaTime);
    void OnEntityEvent(Entity entity, Path scriptPath, Event& event);

    bool InitializeScene(Path scriptPath);
    bool UpdateScene(Path scriptPath, float deltaTime);
    void OnSceneEvent(Path scriptPath, Event& event);

    void ReloadScript(Path scriptPath);

    void InspectEntityScript(Entity entity, Path scriptPath);

    sol::state& GetLua() { return lua; }

    static void SetActiveWorld(Elysium::World* w);

private:
    sol::state lua;

    // Captures the script "Module" or "Class" table.
    // Key: Script Path (e.g. "Scripts/test.lua")
    std::unordered_map<Path, sol::table> scriptRegistry;

    // Active Instances per Entity per Script
    // Key: Entity ID -> Script Path -> Instance table
    std::unordered_map<Entity, std::unordered_map<Path, sol::table>> entityScriptInstances;

    // Active Instances per Scene Script
    // Key: Script Path -> Instance table
    std::unordered_map<Path, sol::table> sceneScriptInstances;

    void InitLuaContext();
    void BindEntityAPI();
    void BindRaylibConstants();
    void BindComponents();

    // Loads the script if not already loaded, returns the table
    sol::table GetOrLoadScript(Path path);

    sol::table GetEntityInstance(Entity entity, Path scriptPath, bool create = false);
    sol::table GetSceneInstance(Path scriptPath, bool create = false);
};

} // namespace Elysium::Services
