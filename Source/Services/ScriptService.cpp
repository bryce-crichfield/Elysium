#include "ScriptService.h"
#include "Core/Application.h"
#include "Services/LogService.h"
#include "Services/SceneService.h"
#include "Core/Scene.h"
#include "Core/Component.h"
#include "Utilities/Path.h"
#include "imgui.h"
#include <memory>

#include "Systems/PickSystem.h"

// =================================================================================================
// Component Adapters
// =================================================================================================
namespace Elysium::Services {

    void PositionComponentAdapter::Get(Elysium::World* world, Elysium::Entity entity, lua_State* L) {
        if (world->HasComponent<Elysium::PositionComponent>(entity)) {
            const auto& pos = world->GetComponent<Elysium::PositionComponent>(entity);
            lua_newtable(L);
            lua_pushnumber(L, pos.x);
            lua_setfield(L, -2, "x");
            lua_pushnumber(L, pos.y);
            lua_setfield(L, -2, "y");
        } else {
            lua_pushnil(L);
        }
    }

    void PositionComponentAdapter::Set(Elysium::World* world, Elysium::Entity entity, lua_State* L) {
        if (lua_istable(L, -1)) {
            Elysium::PositionComponent pos;
            if (world->HasComponent<Elysium::PositionComponent>(entity)) {
                pos = world->GetComponent<Elysium::PositionComponent>(entity);
            }
            lua_getfield(L, -1, "x");
            if (lua_isnumber(L, -1)) {
                pos.x = static_cast<float>(lua_tonumber(L, -1));
            }
            lua_pop(L, 1);
            lua_getfield(L, -1, "y");
            if (lua_isnumber(L, -1)) {
                pos.y = static_cast<float>(lua_tonumber(L, -1));
            }
            lua_pop(L, 1);
            if (world->HasComponent<Elysium::PositionComponent>(entity)) {
                world->GetComponent<Elysium::PositionComponent>(entity) = pos;
            } else {
                world->AddComponent<Elysium::PositionComponent>(entity, pos);
            }
        }
    }

    const char* PositionComponentAdapter::GetComponentName() const {
        return "Position";
    }

    void RectangleComponentAdapter::Get(Elysium::World* world, Elysium::Entity entity, lua_State* L) {
        if (world->HasComponent<Elysium::RectangleComponent>(entity)) {
            const auto& rect = world->GetComponent<Elysium::RectangleComponent>(entity);
            lua_newtable(L);
            lua_pushnumber(L, rect.width);
            lua_setfield(L, -2, "width");
            lua_pushnumber(L, rect.height);
            lua_setfield(L, -2, "height");
            
            lua_newtable(L);
            lua_pushnumber(L, rect.border.r);
            lua_setfield(L, -2, "r");
            lua_pushnumber(L, rect.border.g);
            lua_setfield(L, -2, "g");
            lua_pushnumber(L, rect.border.b);
            lua_setfield(L, -2, "b");
            lua_pushnumber(L, rect.border.a);
            lua_setfield(L, -2, "a");
            lua_setfield(L, -2, "border");

        } else {
            lua_pushnil(L);
        }
    }

    void RectangleComponentAdapter::Set(Elysium::World* world, Elysium::Entity entity, lua_State* L) {
        if (lua_istable(L, -1)) {
            Elysium::RectangleComponent rect;
             if (world->HasComponent<Elysium::RectangleComponent>(entity)) {
                rect = world->GetComponent<Elysium::RectangleComponent>(entity);
            }

            lua_getfield(L, -1, "width");
            if (lua_isnumber(L, -1)) {
                rect.width = static_cast<float>(lua_tonumber(L, -1));
            }
            lua_pop(L, 1);
            lua_getfield(L, -1, "height");
            if (lua_isnumber(L, -1)) {
                rect.height = static_cast<float>(lua_tonumber(L, -1));
            }
            lua_pop(L, 1);

            lua_getfield(L, -1, "border");
            if (lua_istable(L, -1)) {
                lua_getfield(L, -1, "r");
                if (lua_isnumber(L, -1)) rect.border.r = (unsigned char)lua_tointeger(L, -1);
                lua_pop(L, 1);

                lua_getfield(L, -1, "g");
                if (lua_isnumber(L, -1)) rect.border.g = (unsigned char)lua_tointeger(L, -1);
                lua_pop(L, 1);

                lua_getfield(L, -1, "b");
                if (lua_isnumber(L, -1)) rect.border.b = (unsigned char)lua_tointeger(L, -1);
                lua_pop(L, 1);

                lua_getfield(L, -1, "a");
                if (lua_isnumber(L, -1)) rect.border.a = (unsigned char)lua_tointeger(L, -1);
                lua_pop(L, 1);
            }
            lua_pop(L, 1);

            if (world->HasComponent<Elysium::RectangleComponent>(entity)) {
                world->GetComponent<Elysium::RectangleComponent>(entity) = rect;
            } else {
                world->AddComponent<Elysium::RectangleComponent>(entity, rect);
            }
        }
    }

    const char* RectangleComponentAdapter::GetComponentName() const {
        return "Rectangle";
    }

    void MovementComponentAdapter::Get(Elysium::World* world, Elysium::Entity entity, lua_State* L) {
        if (world->HasComponent<Elysium::MovementComponent>(entity)) {
            const auto& mov = world->GetComponent<Elysium::MovementComponent>(entity);
            lua_newtable(L);
            lua_pushnumber(L, mov.speed);
            lua_setfield(L, -2, "speed");
        } else {
            lua_pushnil(L);
        }
    }

    void MovementComponentAdapter::Set(Elysium::World* world, Elysium::Entity entity, lua_State* L) {
        if (lua_istable(L, -1)) {
            Elysium::MovementComponent mov;
            if (world->HasComponent<Elysium::MovementComponent>(entity)) {
                mov = world->GetComponent<Elysium::MovementComponent>(entity);
            }
            lua_getfield(L, -1, "speed");
            if (lua_isnumber(L, -1)) {
                mov.speed = static_cast<float>(lua_tonumber(L, -1));
            }
            lua_pop(L, 1);
            if (world->HasComponent<Elysium::MovementComponent>(entity)) {
                world->GetComponent<Elysium::MovementComponent>(entity) = mov;
            } else {
                world->AddComponent<Elysium::MovementComponent>(entity, mov);
            }
        }
    }

    const char* MovementComponentAdapter::GetComponentName() const {
        return "Movement";
    }

void SpriteComponentAdapter::Get(Elysium::World* world, Elysium::Entity entity, lua_State* L)  {
    if (world->HasComponent<Elysium::SpriteComponent>(entity)) {
        const auto& sprite = world->GetComponent<Elysium::SpriteComponent>(entity);
        lua_newtable(L);
        lua_pushstring(L, sprite.markerName.c_str());
        lua_setfield(L, -2, "marker");
    } else {
        lua_pushnil(L);
    }
}

void SpriteComponentAdapter::Set(Elysium::World* world, Elysium::Entity entity, lua_State* L)  {
    if (lua_istable(L, -1)) {
        Elysium::SpriteComponent sprite;
        if (world->HasComponent<Elysium::SpriteComponent>(entity)) {
            sprite = world->GetComponent<Elysium::SpriteComponent>(entity);
        }
        lua_getfield(L, -1, "marker");
        if (lua_isstring(L, -1)) {
            sprite.markerName = lua_tostring(L, -1);
        }
        lua_pop(L, 1);
        if (world->HasComponent<Elysium::SpriteComponent>(entity)) {
            world->GetComponent<Elysium::SpriteComponent>(entity) = sprite;
        } else {
            world->AddComponent<Elysium::SpriteComponent>(entity, sprite);
        }
    }
}

const char* SpriteComponentAdapter::GetComponentName() const {
    return "Sprite";
}

/*
struct BoundsComponent {
    Rectangle bounds;  // Bounding box in world space
    bool isDragging;   // Is this entity currently being dragged
    Color debugColor;  // Color to draw debug bounds
*/
void BoundsComponentAdapter::Get(Elysium::World* world, Elysium::Entity entity, lua_State* L) {
    if (world->HasComponent<Elysium::BoundsComponent>(entity)) {
        const auto& bounds = world->GetComponent<Elysium::BoundsComponent>(entity);
        lua_newtable(L);
        lua_pushnumber(L, bounds.bounds.x);
        lua_setfield(L, -2, "x");
        lua_pushnumber(L, bounds.bounds.y);
        lua_setfield(L, -2, "y");
        lua_pushnumber(L, bounds.bounds.width);
        lua_setfield(L, -2, "width");
        lua_pushnumber(L, bounds.bounds.height);
        lua_setfield(L, -2, "height");
    } else {
        lua_pushnil(L);
    }
}

void BoundsComponentAdapter::Set(Elysium::World* world, Elysium::Entity entity, lua_State* L) {
    if (lua_istable(L, -1)) {
        Elysium::BoundsComponent bounds;
        if (world->HasComponent<Elysium::BoundsComponent>(entity)) {
            bounds = world->GetComponent<Elysium::BoundsComponent>(entity);
        }
        lua_getfield(L, -1, "x");
        if (lua_isnumber(L, -1)) {
            bounds.bounds.x = static_cast<float>(lua_tonumber(L, -1));
        }
        lua_pop(L, 1);
        lua_getfield(L, -1, "y");
        if (lua_isnumber(L, -1)) {
            bounds.bounds.y = static_cast<float>(lua_tonumber(L, -1));
        }
        lua_pop(L, 1);
        lua_getfield(L, -1, "width");
        if (lua_isnumber(L, -1)) {
            bounds.bounds.width = static_cast<float>(lua_tonumber(L, -1));
        }
        lua_pop(L, 1);
        lua_getfield(L, -1, "height");
        if (lua_isnumber(L, -1)) {
            bounds.bounds.height = static_cast<float>(lua_tonumber(L, -1));
        }
        lua_pop(L, 1);
        if (world->HasComponent<Elysium::BoundsComponent>(entity)) {
            world->GetComponent<Elysium::BoundsComponent>(entity) = bounds;
        } else {
            world->AddComponent<Elysium::BoundsComponent>(entity, bounds);
        }
    }
}

const char* BoundsComponentAdapter::GetComponentName() const {
    return "Bounds";
}


ScriptService::ScriptService() {
    name_ = "ScriptService";
}

ScriptService::~ScriptService() {
}

void ScriptService::Initialize() {
    InitLuaContext();
    RegisterComponentAdapter(std::make_unique<PositionComponentAdapter>());
    RegisterComponentAdapter(std::make_unique<RectangleComponentAdapter>());
    RegisterComponentAdapter(std::make_unique<MovementComponentAdapter>());
    RegisterComponentAdapter(std::make_unique<SpriteComponentAdapter>());
    BindEntityAPI();
    BindRaylibConstants();
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

void ScriptService::RegisterComponentAdapter(std::unique_ptr<ILuaComponentAdapter> adapter) {
    if (!adapter) return;
    const char* name = adapter->GetComponentName();
    componentAdapters[name] = std::move(adapter);
    LOG_INFOF("ScriptService", "Registered component adapter for: %s", name);
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

    // Random(min, max) -> int
    lua_register(L, "Random", [](lua_State* L) -> int {
        int min = (int)luaL_checkinteger(L, 1);
        int max = (int)luaL_checkinteger(L, 2);
        int result = min + (std::rand() % (max - min + 1));
        lua_pushinteger(L, result);
        return 1;
    });

    // SceneReplace(sceneName)
    lua_register(L, "SceneReplace", [](lua_State* L) -> int {
        const char* sceneName = luaL_checkstring(L, 1);
        auto& app = Elysium::Application::GetInstance();
        app.GetService<Elysium::Services::SceneService>().Replace(sceneName);
        return 0;
    });

    // CloneEntity(entityID) -> newEntityID
    lua_register(L, "CloneEntity", [](lua_State* L) -> int {
        Entity entity = (Entity)luaL_checkinteger(L, 1);
        auto* world = GetActiveWorld();
        if (!world) {
            lua_pushinteger(L, 0);
            return 1;
        }
        Entity newEntity = world->CloneEntity(entity);
        lua_pushinteger(L, (lua_Integer)newEntity);
        return 1;
    });

    // GetComponent(entityID, componentName) -> table or nil
    lua_register(L, "GetComponent", [](lua_State* L) -> int {
        Entity entity = (Entity)luaL_checkinteger(L, 1);
        const char* name = luaL_checkstring(L, 2);
        
        ScriptService* service = &Application::GetInstance().GetService<ScriptService>();
        auto it = service->componentAdapters.find(name);
        if (it != service->componentAdapters.end()) {
            it->second->Get(GetActiveWorld(), entity, L);
            return 1;
        }
        return 0;
    });

    // Lua: SetComponent(id, "Position", {x = 10, y = 20})
    lua_register(L, "SetComponent", [](lua_State* L) -> int {
        Entity entity = (Entity)luaL_checkinteger(L, 1);
        const char* name = luaL_checkstring(L, 2);
        // Table is at index 3
        
        ScriptService* service = &Application::GetInstance().GetService<ScriptService>();
        auto it = service->componentAdapters.find(name);
        if (it != service->componentAdapters.end()) {
            lua_pushvalue(L, 3); // Ensure table is at top for Set()
            it->second->Set(GetActiveWorld(), entity, L);
            lua_pop(L, 1);
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

void ScriptService::BindRaylibConstants() {
    if (!L) return;

    // Helper to set a global integer
    auto setInt = [&](const char* name, int value) {
        lua_pushinteger(L, value);
        lua_setglobal(L, name);
    };

    // Keyboard Keys (Common ones)
    setInt("KEY_SPACE", 32);
    setInt("KEY_ENTER", 257);
    setInt("KEY_TAB", 258);
    setInt("KEY_ESCAPE", 256);
    setInt("KEY_BACKSPACE", 259);
    setInt("KEY_LEFT", 263);
    setInt("KEY_RIGHT", 262);
    setInt("KEY_UP", 265);
    setInt("KEY_DOWN", 264);
    
    // Letters
    for (int i = 0; i < 26; ++i) {
        char name[8];
        snprintf(name, sizeof(name), "KEY_%c", 'A' + i);
        setInt(name, 65 + i);
    }

    // Numbers
    for (int i = 0; i < 10; ++i) {
        char name[8];
        snprintf(name, sizeof(name), "KEY_%d", i);
        setInt(name, 48 + i);
    }

    // Mouse Buttons
    setInt("MOUSE_LEFT", 0);
    setInt("MOUSE_RIGHT", 1);
    setInt("MOUSE_MIDDLE", 2);
    setInt("MOUSE_SIDE", 3);
    setInt("MOUSE_EXTRA", 4);
    setInt("MOUSE_FORWARD", 5);
    setInt("MOUSE_BACK", 6);
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
        LOG_WARNINGF("ScriptService", "Script %s did not return a table.", scriptName.c_str());
        lua_pop(L, 1);
        return LUA_REFNIL;
    }

    int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    scriptRegistry[scriptName] = ref;
    LOG_INFOF("ScriptService", "Loaded script table: %s", scriptName.c_str());
    return ref;
}

int ScriptService::GetEntityInstance(Entity entity, const std::string& scriptName, bool create) {
    auto it = entityScriptInstances.find(entity);
    if (it != entityScriptInstances.end()) {
        return it->second;
    }

    if (!create) return LUA_REFNIL;

    // Create new instance
    int templateRef = GetOrLoadScript(scriptName);
    if (templateRef == LUA_REFNIL) return LUA_REFNIL;

    // 1. New Table (Instance)
    lua_newtable(L); 
    
    // 2. New Table (Metatable)
    lua_newtable(L); 
    
    // 3. Get Template
    lua_rawgeti(L, LUA_REGISTRYINDEX, templateRef); 
    
    // 4. Metatable.__index = Template
    lua_setfield(L, -2, "__index"); 
    
    // 5. Set Metatable
    lua_setmetatable(L, -2);

    // 6. Store Instance
    int instanceRef = luaL_ref(L, LUA_REGISTRYINDEX);
    entityScriptInstances[entity] = instanceRef;
    
    return instanceRef;
}

bool ScriptService::InitEntity(Entity entity, const std::string& scriptName) {
    if (!L) return false;

    // Force creation of instance
    int ref = GetEntityInstance(entity, scriptName, true);
    if (ref == LUA_REFNIL) return false;

    lua_rawgeti(L, LUA_REGISTRYINDEX, ref); // Stack: [Instance]
    lua_getfield(L, -1, "init");            // Stack: [Instance, Function?]
    
    bool result = true;
    if (lua_isfunction(L, -1)) {
        lua_pushvalue(L, -2); // Push Instance as 'self'
        lua_pushinteger(L, (lua_Integer)entity);
        if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
             const char* err = lua_tostring(L, -1);
             LOG_ERRORF("ScriptService", "Error in %s:init: %s", scriptName.c_str(), err);
             lua_pop(L, 1);
             result = false;
        }
    } else {
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    return result;
}

bool ScriptService::UpdateEntity(Entity entity, const std::string& scriptName, float deltaTime) {
    if (!L) return false;

    // Get existing instance
    int ref = GetEntityInstance(entity, scriptName, false);
    if (ref == LUA_REFNIL) return false;

    lua_rawgeti(L, LUA_REGISTRYINDEX, ref); // Stack: [Instance]
    lua_getfield(L, -1, "update");          // Stack: [Instance, Function?]
    
    if (lua_isfunction(L, -1)) {
        lua_pushvalue(L, -2); // Push Instance as 'self'
        lua_pushinteger(L, (lua_Integer)entity);
        lua_pushnumber(L, (lua_Number)deltaTime);
        if (lua_pcall(L, 3, 0, 0) != LUA_OK) {
             const char* err = lua_tostring(L, -1);
             LOG_ERRORF("ScriptService", "Error in %s:update: %s", scriptName.c_str(), err);
             lua_pop(L, 1);
        }
    } else {
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    return true;
}

void ScriptService::OnEntityEvent(Entity entity, const std::string& scriptName, Event& event) {
    if (!L) return;

    int ref = GetEntityInstance(entity, scriptName, false);
    if (ref == LUA_REFNIL) return;

    lua_rawgeti(L, LUA_REGISTRYINDEX, ref); // Stack: [Instance]
    lua_getfield(L, -1, "onEvent");         // Stack: [Instance, Function?]
    
    if (lua_isfunction(L, -1)) {
        lua_pushvalue(L, -2); // Push Instance as 'self'
        lua_pushinteger(L, (lua_Integer)entity);
        lua_newtable(L);

        // Helper to add World Coordinates for mouse events
        auto AddWorldCoords = [&](float screenX, float screenY) {
            auto* world = GetActiveWorld();
            if (world) {
                // Find active camera
                Vector2 cameraPos = {0, 0};
                float zoom = 1.0f;
                Vector2 viewportCenter = {0, 0};
                bool foundCamera = false;

                world->Query<CameraComponent>([&](Entity camEnt, auto& cameraComp) {
                    if (!foundCamera && cameraComp.isVisible) {
                        if (world->HasComponent<PositionComponent>(camEnt)) {
                            auto& pos = world->GetComponent<PositionComponent>(camEnt);
                            cameraPos = { pos.x, pos.y };
                        }
                        zoom = cameraComp.zoom;
                        viewportCenter = { cameraComp.viewport.width * 0.5f, cameraComp.viewport.height * 0.5f };
                        foundCamera = true;
                    }
                });

                if (foundCamera) {
                    // Inverse Transform: Screen -> World
                    // 1. Translate to center relative (Screen - ViewportCenter)
                    float cx = screenX - viewportCenter.x;
                    float cy = screenY - viewportCenter.y;
                    
                    // 2. Scale Inverse ( / Zoom )
                    cx /= zoom;
                    cy /= zoom;
                    
                    // 3. Camera Translate Inverse ( + CameraPos )
                    float wx = cx + cameraPos.x;
                    float wy = cy + cameraPos.y;

                    lua_pushnumber(L, wx);
                    lua_setfield(L, -2, "wx");
                    lua_pushnumber(L, wy);
                    lua_setfield(L, -2, "wy");
                    return;
                }
            }
            // Fallback if no camera found: World = Screen
            lua_pushnumber(L, screenX);
            lua_setfield(L, -2, "wx");
            lua_pushnumber(L, screenY);
            lua_setfield(L, -2, "wy");
        };

        if (auto* e = event.As<KeyPressedEvent>()) {
            lua_pushstring(L, "KeyPressed");
            lua_setfield(L, -2, "type");
            lua_pushinteger(L, e->GetKey());
            lua_setfield(L, -2, "key");
        }
        else if (auto* e = event.As<KeyReleasedEvent>()) {
            lua_pushstring(L, "KeyReleased");
            lua_setfield(L, -2, "type");
            lua_pushinteger(L, e->GetKey());
            lua_setfield(L, -2, "key");
        }
        else if (auto* e = event.As<MouseButtonPressedEvent>()) {
            lua_pushstring(L, "MouseButtonPressed");
            lua_setfield(L, -2, "type");
            lua_pushinteger(L, e->GetButton());
            lua_setfield(L, -2, "button");
            lua_pushnumber(L, e->GetPosition().x);
            lua_setfield(L, -2, "x");
            lua_pushnumber(L, e->GetPosition().y);
            lua_setfield(L, -2, "y");
            AddWorldCoords(e->GetPosition().x, e->GetPosition().y);
        }
        else if (auto* e = event.As<MouseButtonReleasedEvent>()) {
            lua_pushstring(L, "MouseButtonReleased");
            lua_setfield(L, -2, "type");
            lua_pushinteger(L, e->GetButton());
            lua_setfield(L, -2, "button");
            lua_pushnumber(L, e->GetPosition().x);
            lua_setfield(L, -2, "x");
            lua_pushnumber(L, e->GetPosition().y);
            lua_setfield(L, -2, "y");
            AddWorldCoords(e->GetPosition().x, e->GetPosition().y);
        }
        else if (auto* e = event.As<MouseMovedEvent>()) {
            lua_pushstring(L, "MouseMoved");
            lua_setfield(L, -2, "type");
            lua_pushnumber(L, e->GetPosition().x);
            lua_setfield(L, -2, "x");
            lua_pushnumber(L, e->GetPosition().y);
            lua_setfield(L, -2, "y");
            lua_pushnumber(L, e->GetDelta().x);
            lua_setfield(L, -2, "dx");
            lua_pushnumber(L, e->GetDelta().y);
            lua_setfield(L, -2, "dy");
            AddWorldCoords(e->GetPosition().x, e->GetPosition().y);
        }
        else if (auto* e = event.As<Elysium::Systems::PickEvent>()) {
            std::string typeStr = "";
            switch (e->type) {
                case Elysium::Systems::PickEvent::Type::PRESS:
                    typeStr = "PickPress";
                    break;
                case Elysium::Systems::PickEvent::Type::RELEASE:
                    typeStr = "PickRelease";
                    break;
                case Elysium::Systems::PickEvent::Type::MOVE:
                    typeStr = "PickMove";
                    break;
            }
            lua_pushstring(L, typeStr.c_str());
            lua_setfield(L, -2, "type");
            lua_pushnumber(L, e->position.x);
            lua_setfield(L, -2, "wx");
            lua_pushnumber(L, e->position.y);
            lua_setfield(L, -2, "wy");
        }
        else {
            // Unknown event or not handled yet
             lua_pushstring(L, "Unknown");
             lua_setfield(L, -2, "type");
        }

        if (lua_pcall(L, 3, 0, 0) != LUA_OK) {
             const char* err = lua_tostring(L, -1);
             LOG_ERRORF("ScriptService", "Error in %s:onEvent: %s", scriptName.c_str(), err);
             lua_pop(L, 1);
        }
    } else {
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

void ScriptService::ReloadScript(const std::string& scriptName) {
    auto it = scriptRegistry.find(scriptName);
    if (it != scriptRegistry.end()) {
        luaL_unref(L, LUA_REGISTRYINDEX, it->second);
        scriptRegistry.erase(it);
        LOG_INFOF("ScriptService", "Unloaded script: %s. Note: Existing instances retain old behavior until reset.", scriptName.c_str());
    }
}

void ScriptService::InspectEntityScript(Entity entity) {
    auto it = entityScriptInstances.find(entity);
    if (it == entityScriptInstances.end()) {
        ImGui::TextDisabled("No script instance.");
        return;
    }

    if (!L) return;

    int ref = it->second;
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref); // Stack: [Instance]

    if (lua_istable(L, -1)) {
        lua_pushnil(L); // first key
        while (lua_next(L, -2) != 0) {
            // Stack: [Instance, Key, Value]
            const char* key = lua_tostring(L, -2);
            if (key && key[0] != '_') { // Skip internal/meta keys
                ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "%s:", key);
                ImGui::SameLine();

                if (lua_isnumber(L, -1)) {
                    float val = (float)lua_tonumber(L, -1);
                    ImGui::Text("%.2f", val);
                } else if (lua_isstring(L, -1)) {
                    ImGui::Text("%s", lua_tostring(L, -1));
                } else if (lua_isboolean(L, -1)) {
                    ImGui::Text("%s", lua_toboolean(L, -1) ? "true" : "false");
                } else if (lua_isfunction(L, -1)) {
                    ImGui::TextDisabled("(function)");
                } else if (lua_istable(L, -1)) {
                    ImGui::TextDisabled("(table)");
                } else {
                    ImGui::TextDisabled("?");
                }
            }
            lua_pop(L, 1); // Remove value, keep key for next
        }
    }
    
    lua_pop(L, 1); // Pop Instance
}

} // namespace Elysium::Services
