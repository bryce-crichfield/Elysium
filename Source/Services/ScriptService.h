#pragma once

#include "Core/ServiceLocator.h"
#include "Interfaces/IScriptService.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include "Core/Entity.h"
#include "Core/Event.h"
#include "Core/Path.h"
#include <sol/sol.hpp>

namespace Elysium::Services {

class ScriptService : public IScriptService {
public:
    ScriptService(ServiceLocator& registry);
    ~ScriptService() override;

    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;

    // Core execution
    Elysium::ScriptResult ExecuteString(const std::string& scriptString) override;
    std::vector<Entity> FilterEntities(const std::string& filterFunctionBody) override;

    bool InitializeEntity(Entity entity, Path scriptPath) override;
    bool UpdateEntity(Entity entity, Path scriptPath, float deltaTime) override;
    void OnEntityEvent(Entity entity, Path scriptPath, Event& event) override;

    bool InitializeScene(Path scriptPath) override;
    bool UpdateScene(Path scriptPath, float deltaTime) override;
    bool RenderScene(Path scriptPath) override;
    void OnSceneEvent(Path scriptPath, Event& event) override;

    void ReloadScript(Path scriptPath) override;

    std::optional<std::vector<ScriptField>> GetScriptFields(Entity entity, Path scriptPath) override;
    bool SetScriptField(Entity entity, Path scriptPath, const std::string& name, const ScriptValue& value) override;

    sol::state& GetLua() { return lua; }

    void SetActiveWorld(Elysium::World* w) override;

    void SetMousePosition(float x, float y) override { _mousePosition = {x, y}; }
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

    // Scene hooks already reported missing, so the warning is once per script rather than once
    // per frame. Key: "<script path>:<hook>".
    std::unordered_set<std::string> warnedMissingHooks_;
    // Warns the first time `hook` is missing on a scene script; returns false so callers can
    // `return WarnMissingSceneHook(...)` in place of a silent `return false`.
    bool WarnMissingSceneHook(const Path& scriptPath, const char* hook);

    void InitLuaContext();
    void BindEntityAPI();
    void BindInputConstants();
    void BindComponents();

    // Loads the script if not already loaded, returns the table
    sol::table GetOrLoadScript(Path path);

    sol::table GetEntityInstance(Entity entity, Path scriptPath, bool create = false);
    sol::table GetSceneInstance(Path scriptPath, bool create = false);

    Vector2 _mousePosition;
};

} // namespace Elysium::Services
