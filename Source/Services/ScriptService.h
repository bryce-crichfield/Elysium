#pragma once

#include "Service.h"
#include <string>
#include <unordered_map>
#include "Core/Entity.h"
#include "Core/Event.h"
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
    void ExecuteString(const std::string& scriptString);
    
    // Entity Scripting System
    // Returns true if the script was loaded and has an 'update' function
    bool UpdateEntity(Entity entity, const std::string& scriptName, float deltaTime);
    
    // Returns true if the script was loaded and Init was called (or didn't exist)
    bool InitEntity(Entity entity, const std::string& scriptName);

    // Handle events
    void OnEntityEvent(Entity entity, const std::string& scriptName, Event& event);

    // Force reload of a script from disk (hot-reloading)
    void ReloadScript(const std::string& scriptName);

    // Editor Helpers
    void InspectEntityScript(Entity entity);

    sol::state& GetLua() { return lua; }

private:
    sol::state lua;
    
    // Captures the script "Module" or "Class" table.
    // Key: Script Path (e.g. "Scripts/test.lua")
    std::unordered_map<std::string, sol::table> scriptRegistry;

    // Active Instances per Entity
    // Key: Entity ID
    std::unordered_map<Entity, sol::table> entityScriptInstances;

    void InitLuaContext();
    void BindEntityAPI();
    void BindRaylibConstants();
    void BindComponents();
    
    // Loads the script if not already loaded, returns the table
    sol::table GetOrLoadScript(const std::string& scriptName);
    
    // Helper to get or create the instance for an entity
    // If create is true, it will instantiate from the script template
    sol::table GetEntityInstance(Entity entity, const std::string& scriptName, bool create = false);
};

} // namespace Elysium::Services