#include "ScriptService.h"
#include "Core/Application.h"
#include "Services/LogService.h"
#include "Services/SceneService.h"
#include "Core/Scene.h"
#include "Core/Component.h"
#include "Utilities/Path.h"

namespace Elysium::Services {

ScriptService::ScriptService() {
    name_ = "ScriptService";
}

ScriptService::~ScriptService() {
}

void ScriptService::Initialize() {
    InitLuaContext();
    BindEntityAPI();
    LOG_INFO("ScriptService", "Lua VM Initialized with Table Capture support");
}

void ScriptService::Shutdown() {
    if (L) {
        lua_close(L);
        L = nullptr;
    }
    LOG_INFO("ScriptService", "Lua VM Shutdown");
}

void ScriptService::Update(float deltaTime) {
}

void ScriptService::ExecuteString(const std::string& scriptString) {
    if (luaL_dostring(L, scriptString.c_str()) != LUA_OK) {
        const char* error = lua_tostring(L, -1);
        LOG_ERRORF("ScriptService", "Lua Error: %s", error);
        lua_pop(L, 1);
    }
}

void ScriptService::RegisterFunction(const std::string& name, lua_CFunction func) {
    if (L) {
        lua_register(L, name.c_str(), func);
    }
}

void ScriptService::InitLuaContext() {
    if (L) return;
    L = luaL_newstate();
    luaL_openlibs(L);
}

// Helper to get World from active scene
static Elysium::World* GetActiveWorld() {
    auto& app = Elysium::Application::GetInstance();
    auto* scene = app.GetService<Elysium::Services::SceneService>().GetScene();
    if (scene) {
        return scene->GetWorld();
    }
    return nullptr;
}

void ScriptService::BindEntityAPI() {
    // Expose a global 'Log' function
    lua_register(L, "Log", [](lua_State* L) -> int {
        const char* msg = luaL_checkstring(L, 1);
        LOG_INFO("Lua", msg);
        return 0;
    });

    // GetPosition(entityID) -> x, y
    lua_register(L, "GetPosition", [](lua_State* L) -> int {
        int entityID = (int)luaL_checkinteger(L, 1);
        auto* world = GetActiveWorld();
        if (world && world->HasComponent<PositionComponent>(entityID)) {
            const auto& pos = world->GetComponent<PositionComponent>(entityID);
            lua_pushnumber(L, pos.x);
            lua_pushnumber(L, pos.y);
            return 2;
        }
        return 0; // Return nil if invalid
    });

    // SetPosition(entityID, x, y)
    lua_register(L, "SetPosition", [](lua_State* L) -> int {
        int entityID = (int)luaL_checkinteger(L, 1);
        float x = (float)luaL_checknumber(L, 2);
        float y = (float)luaL_checknumber(L, 3);
        
        auto* world = GetActiveWorld();
        if (world) {
            if(world->HasComponent<PositionComponent>(entityID)) {
                auto& pos = world->GetComponent<PositionComponent>(entityID);
                pos.x = x;
                pos.y = y;
            } else {
                 // Optional: Add component if it doesn't exist?
                 // For now, let's enforce it must exist
                 LOG_WARNINGF("Lua", "Entity %d has no PositionComponent", entityID);
            }
        }
        return 0;
    });

    // GetRectangle(entityID) -> width, height
    lua_register(L, "GetRectangle", [](lua_State* L) -> int {
        int entityID = (int)luaL_checkinteger(L, 1);
        auto* world = GetActiveWorld();
        if (world && world->HasComponent<RectangleComponent>(entityID)) {
            const auto& rect = world->GetComponent<RectangleComponent>(entityID);
            lua_pushnumber(L, rect.width);
            lua_pushnumber(L, rect.height);
            return 2;
        }
        return 0; 
    });

    // SetRectangle(entityID, width, height)
    lua_register(L, "SetRectangle", [](lua_State* L) -> int {
        int entityID = (int)luaL_checkinteger(L, 1);
        float w = (float)luaL_checknumber(L, 2);
        float h = (float)luaL_checknumber(L, 3);
        
        auto* world = GetActiveWorld();
        if (world && world->HasComponent<RectangleComponent>(entityID)) {
            auto& rect = world->GetComponent<RectangleComponent>(entityID);
            rect.width = w;
            rect.height = h;
        }
        return 0;
    });

    // AddComponent(entityID, componentName)
    lua_register(L, "AddComponent", [](lua_State* L) -> int {
        int entityID = (int)luaL_checkinteger(L, 1);
        const char* typeName = luaL_checkstring(L, 2);
        std::string type = typeName;

        auto* world = GetActiveWorld();
        if (!world) return 0;

        // Simple string mapping factory
        if (type == "Position") {
            if (!world->HasComponent<PositionComponent>(entityID))
                world->AddComponent(entityID, PositionComponent());
        }
        else if (type == "Rectangle") {
             if (!world->HasComponent<RectangleComponent>(entityID))
                world->AddComponent(entityID, RectangleComponent());
        }
        else if (type == "Movement") {
             if (!world->HasComponent<MovementComponent>(entityID))
                world->AddComponent(entityID, MovementComponent());
        }
        else if (type == "Sprite") {
             if (!world->HasComponent<SpriteComponent>(entityID))
                world->AddComponent(entityID, SpriteComponent());
        }
        else {
            LOG_WARNINGF("Lua", "AddComponent: Unknown component type '%s'", typeName);
        }

        return 0;
    });
}

int ScriptService::GetOrLoadScript(const std::string& scriptName) {
    auto it = scriptRegistry.find(scriptName);
    if (it != scriptRegistry.end()) {
        return it->second;
    }

    // Resolve path using Elysium::Path
    Path path(scriptName);
    
    // Load file
    if (luaL_loadfile(L, path.c_str()) != LUA_OK) {
        LOG_ERRORF("ScriptService", "Failed to load script file: %s (Path: %s) Error: %s", 
            scriptName.c_str(), path.c_str(), lua_tostring(L, -1));
        lua_pop(L, 1);
        return LUA_REFNIL;
    }

    // Execute the chunk (the file) to get the returned table
    if (lua_pcall(L, 0, 1, 0) != LUA_OK) {
        LOG_ERRORF("ScriptService", "Failed to execute script chunk: %s Error: %s", scriptName.c_str(), lua_tostring(L, -1));
        lua_pop(L, 1);
        return LUA_REFNIL;
    }

    // Check if it returned a table
    if (!lua_istable(L, -1)) {
        // If it didn't return a table, it might be a simple script. 
        // We can't use it for the component system effectively in this design.
        LOG_WARNINGF("ScriptService", "Script %s did not return a table. It cannot be used as a component script.", scriptName.c_str());
        lua_pop(L, 1); // Pop the non-table result
        return LUA_REFNIL;
    }

    // Store in registry
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    scriptRegistry[scriptName] = ref;
    
    LOG_INFOF("ScriptService", "Loaded script table: %s", scriptName.c_str());
    return ref;
}

bool ScriptService::InitEntity(Entity entity, const std::string& scriptName) {
    if (!L) return false;

    int ref = GetOrLoadScript(scriptName);
    if (ref == LUA_REFNIL) return false;

    // Push the script table
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref); // Stack: [Table]
    
    // Get 'init' function
    lua_getfield(L, -1, "init"); // Stack: [Table, Function?]
    
    bool result = true;

    if (lua_isfunction(L, -1)) {
        // Call: init(entityID)
        lua_pushinteger(L, (lua_Integer)entity); // Arg1: Entity ID
        
        // pcall(nargs, nresults, errfunc)
        if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
             const char* err = lua_tostring(L, -1);
             LOG_ERRORF("ScriptService", "Error in %s:init: %s", scriptName.c_str(), err);
             lua_pop(L, 1); // Pop error
             result = false;
        }
    } else {
        // Pop the non-function value
        lua_pop(L, 1);
        // It's okay if init doesn't exist
    }
    
    // Pop the script table
    lua_pop(L, 1);
    return result;
}

bool ScriptService::UpdateEntity(Entity entity, const std::string& scriptName, float deltaTime) {
    if (!L) return false;

    int ref = GetOrLoadScript(scriptName);
    if (ref == LUA_REFNIL) return false;

    // Push the script table
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref); // Stack: [Table]
    
    // Get 'update' function
    lua_getfield(L, -1, "update"); // Stack: [Table, Function?]
    
    if (lua_isfunction(L, -1)) {
        // Call: update(entityID, dt)
        lua_pushinteger(L, (lua_Integer)entity); // Arg1: Entity ID
        lua_pushnumber(L, (lua_Number)deltaTime); // Arg2: DeltaTime
        
        // pcall(nargs, nresults, errfunc)
        if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
             const char* err = lua_tostring(L, -1);
             LOG_ERRORF("ScriptService", "Error in %s:update: %s", scriptName.c_str(), err);
             lua_pop(L, 1); // Pop error
        }
    } else {
        // Pop the non-function value
        lua_pop(L, 1);
    }
    
    // Pop the script table
    lua_pop(L, 1);
    return true;
}

void ScriptService::ReloadScript(const std::string& scriptName) {
    auto it = scriptRegistry.find(scriptName);
    if (it != scriptRegistry.end()) {
        luaL_unref(L, LUA_REGISTRYINDEX, it->second);
        scriptRegistry.erase(it);
        LOG_INFOF("ScriptService", "Unloaded script: %s", scriptName.c_str());
    }
    // It will be reloaded on next GetOrLoadScript call
}

} // namespace Elysium::Services
