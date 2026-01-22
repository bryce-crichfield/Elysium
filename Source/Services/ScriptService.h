#pragma once

#include "Service.h"
#include <string>
#include <unordered_map>
#include "Core/Entity.h"

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

namespace Elysium::Services {

class ScriptService : public Elysium::Service {
public:
    ScriptService();
    ~ScriptService() override;

    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;

    // Core execution (legacy/direct)
    void ExecuteString(const std::string& scriptString);
    
    // API Registration
    void RegisterFunction(const std::string& name, lua_CFunction func);

    // Entity Scripting System
    // Returns true if the script was loaded and has an 'update' function
    bool UpdateEntity(Entity entity, const std::string& scriptName, float deltaTime);

    // Force reload of a script from disk (hot-reloading)
    void ReloadScript(const std::string& scriptName);

private:
    lua_State* L = nullptr;
    
    // Captures the script "Module" or "Class" table.
    // Key: Script Path (e.g. "Scripts/test.lua")
    // Value: Lua Registry Reference (int) to the table returned by the script
    std::unordered_map<std::string, int> scriptRegistry;

    void InitLuaContext();
    void BindEntityAPI();
    
    // Loads the script if not already loaded, returns the Registry Reference
    int GetOrLoadScript(const std::string& scriptName);
};

} // namespace Elysium::Services
