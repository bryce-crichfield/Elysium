#define SOL_HEADER_ONLY 1
#define SOL_ALL_SAFETIES_ON 1
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

namespace Elysium::Services {

ScriptService::ScriptService() {
    name_ = "ScriptService";
}

ScriptService::~ScriptService() {
}

void ScriptService::Initialize() {
    InitLuaContext();
    BindComponents();
    BindEntityAPI();
    BindRaylibConstants();
    LOG_INFO("ScriptService", "Lua VM Initialized with sol2 and component usertypes");
}

void ScriptService::Shutdown() {
    LOG_INFO("ScriptService", "Lua VM Shutdown");
}

void ScriptService::Update(float deltaTime) {
}

void ScriptService::ExecuteString(const std::string& scriptString) {
    auto result = lua.safe_script(scriptString, sol::script_pass_on_error);
    if (!result.valid()) {
        sol::error err = result;
        LOG_ERRORF("ScriptService", "Lua Error: %s", err.what());
    }
}

void ScriptService::InitLuaContext() {
    lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::table, sol::lib::string, sol::lib::math, sol::lib::debug);
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

static Color TableToColor(const sol::table& t) {
    return Color{
        (unsigned char)t.get_or("r", 255),
        (unsigned char)t.get_or("g", 255),
        (unsigned char)t.get_or("b", 255),
        (unsigned char)t.get_or("a", 255)
    };
}

static Color ObjectToColor(const sol::object& obj) {
    if (obj.is<Color>()) return obj.as<Color>();
    if (obj.is<sol::table>()) return TableToColor(obj.as<sol::table>());
    return WHITE;
}

static Vector2 ScreenToWorld(Vector2 screenPos) {
    auto* world = GetActiveWorld();
    if (!world) return screenPos;

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
        return {
            ((screenPos.x - viewportCenter.x) / zoom) + cameraPos.x,
            ((screenPos.y - viewportCenter.y) / zoom) + cameraPos.y
        };
    }
    return screenPos;
}

void ScriptService::BindComponents() {
    // Vector2 (Raylib)
    auto vec2 = lua.new_usertype<Vector2>("Vector2", sol::constructors<Vector2(), Vector2(float, float)>());
    vec2["x"] = &Vector2::x;
    vec2["y"] = &Vector2::y;

    // Color (Raylib)
    auto col = lua.new_usertype<Color>("Color", sol::constructors<Color()>());
    col["r"] = &Color::r;
    col["g"] = &Color::g;
    col["b"] = &Color::b;
    col["a"] = &Color::a;

    // Rectangle (Raylib)
    auto rect = lua.new_usertype<Rectangle>("Rectangle", sol::constructors<Rectangle()>());
    rect["x"] = &Rectangle::x;
    rect["y"] = &Rectangle::y;
    rect["width"] = &Rectangle::width;
    rect["height"] = &Rectangle::height;

    // PositionComponent
    auto posComp = lua.new_usertype<PositionComponent>("PositionComponent", sol::constructors<PositionComponent(), PositionComponent(float, float)>());
    posComp["x"] = &PositionComponent::x;
    posComp["y"] = &PositionComponent::y;

    // NameComponent
    auto nameComp = lua.new_usertype<NameComponent>("NameComponent", sol::constructors<NameComponent(), NameComponent(std::string)>());
    nameComp["name"] = &NameComponent::name;

    // RectangleComponent
    auto rectComp = lua.new_usertype<RectangleComponent>("RectangleComponent", sol::constructors<RectangleComponent()>());
    rectComp["width"] = &RectangleComponent::width;
    rectComp["height"] = &RectangleComponent::height;
    rectComp["background"] = sol::property([](RectangleComponent& r) { return r.background; }, [](RectangleComponent& r, sol::object v) { r.background = ObjectToColor(v); });
    rectComp["border"] = sol::property([](RectangleComponent& r) { return r.border; }, [](RectangleComponent& r, sol::object v) { r.border = ObjectToColor(v); });
    rectComp["layerName"] = &RectangleComponent::layerName;

    // MovementComponent
    auto moveComp = lua.new_usertype<MovementComponent>("MovementComponent", sol::constructors<MovementComponent()>());
    moveComp["speed"] = &MovementComponent::speed;
    moveComp["isMoving"] = &MovementComponent::isMoving;
    moveComp["loop"] = &MovementComponent::loop;
    moveComp["AddWaypoint"] = &MovementComponent::AddWaypoint;
    moveComp["ClearWaypoints"] = &MovementComponent::ClearWaypoints;

    // SpriteComponent
    auto spriteComp = lua.new_usertype<SpriteComponent>("SpriteComponent", sol::constructors<SpriteComponent()>());
    spriteComp["marker"] = &SpriteComponent::markerName;
    spriteComp["layer"] = &SpriteComponent::layerName;

    // BoundsComponent
    auto boundsComp = lua.new_usertype<BoundsComponent>("BoundsComponent", sol::constructors<BoundsComponent()>());
    boundsComp["bounds"] = &BoundsComponent::bounds;
    boundsComp["isDragging"] = &BoundsComponent::isDragging;
    boundsComp["debugColor"] = sol::property([](BoundsComponent& b) { return b.debugColor; }, [](BoundsComponent& b, sol::object v) { b.debugColor = ObjectToColor(v); });

    // TeamComponent
    auto teamComp = lua.new_usertype<TeamComponent>("TeamComponent", sol::constructors<TeamComponent(), TeamComponent(int)>());
    teamComp["teamId"] = &TeamComponent::teamId;

    // UnitComponent
    auto unitComp = lua.new_usertype<UnitComponent>("UnitComponent", sol::constructors<UnitComponent()>());
    unitComp["hasActedThisTurn"] = &UnitComponent::hasActedThisTurn;
    unitComp["canMove"] = &UnitComponent::canMove;
    unitComp["canAttack"] = &UnitComponent::canAttack;

    // ScriptComponent
    auto scriptComp = lua.new_usertype<ScriptComponent>("ScriptComponent", sol::constructors<ScriptComponent(), ScriptComponent(std::string)>());
    scriptComp["scriptName"] = &ScriptComponent::scriptName;
    scriptComp["isActive"] = &ScriptComponent::isActive;
    scriptComp["isInitialized"] = &ScriptComponent::isInitialized;
}

void ScriptService::BindEntityAPI() {
    // Log
    lua.set_function("Log", [](const std::string& msg) {
        LOG_INFO("Lua", msg.c_str());
    });

    // Lifecycle
    lua.set_function("CreateEntity", []() -> Entity {
        auto* world = GetActiveWorld();
        return world ? world->CreateEntity() : 0;
    });

    lua.set_function("DestroyEntity", [](Entity entity) {
        auto* world = GetActiveWorld();
        if (world) world->DestroyEntity(entity);
    });

    lua.set_function("CloneEntity", [](Entity entity) {
        auto* world = GetActiveWorld();
        return world ? world->CloneEntity(entity) : 0;
    });

    // Random
    lua.set_function("Random", [](int min, int max) {
        return min + (std::rand() % (max - min + 1));
    });

    // Scene
    lua.set_function("SceneReplace", [](const std::string& sceneName) {
        auto& app = Elysium::Application::GetInstance();
        app.GetService<Elysium::Services::SceneService>().Replace(sceneName);
    });

    // Input Polling
    lua.set_function("IsKeyDown", [](int key) { return IsKeyDown(key); });
    lua.set_function("IsKeyPressed", [](int key) { return IsKeyPressed(key); });
    lua.set_function("IsMouseButtonDown", [](int button) { return IsMouseButtonDown(button); });
    lua.set_function("IsMouseButtonPressed", [](int button) { return IsMouseButtonPressed(button); });
    lua.set_function("GetMousePosition", []() { 
        Vector2 m = GetMousePosition();
        return ScreenToWorld(m);
    });

    // GetComponent
    lua.set_function("GetComponent", [this](Entity entity, const std::string& name) -> sol::object {
        auto* world = GetActiveWorld();
        if (!world) return sol::nil;

        if (name == "Position") {
            if (world->HasComponent<PositionComponent>(entity))
                return sol::make_object(lua.lua_state(), &world->GetComponent<PositionComponent>(entity));
        }
        else if (name == "Name") {
            if (world->HasComponent<NameComponent>(entity))
                return sol::make_object(lua.lua_state(), &world->GetComponent<NameComponent>(entity));
        }
        else if (name == "Rectangle") {
            if (world->HasComponent<RectangleComponent>(entity))
                return sol::make_object(lua.lua_state(), &world->GetComponent<RectangleComponent>(entity));
        }
        else if (name == "Movement") {
            if (world->HasComponent<MovementComponent>(entity))
                return sol::make_object(lua.lua_state(), &world->GetComponent<MovementComponent>(entity));
        }
        else if (name == "Sprite") {
            if (world->HasComponent<SpriteComponent>(entity))
                return sol::make_object(lua.lua_state(), &world->GetComponent<SpriteComponent>(entity));
        }
        else if (name == "Bounds") {
            if (world->HasComponent<BoundsComponent>(entity))
                return sol::make_object(lua.lua_state(), &world->GetComponent<BoundsComponent>(entity));
        }
        else if (name == "Team") {
            if (world->HasComponent<TeamComponent>(entity))
                return sol::make_object(lua.lua_state(), &world->GetComponent<TeamComponent>(entity));
        }
        else if (name == "Unit") {
            if (world->HasComponent<UnitComponent>(entity))
                return sol::make_object(lua.lua_state(), &world->GetComponent<UnitComponent>(entity));
        }
        else if (name == "Script") {
            if (world->HasComponent<ScriptComponent>(entity))
                return sol::make_object(lua.lua_state(), &world->GetComponent<ScriptComponent>(entity));
        }
        return sol::nil;
    });

    // SetComponent
    lua.set_function("SetComponent", [](Entity entity, const std::string& name, sol::object value) {
        auto* world = GetActiveWorld();
        if (!world) return;

        auto getOrAdd = [&]<typename T>() -> T& {
            if (!world->HasComponent<T>(entity)) {
                world->AddComponent<T>(entity, T());
            }
            return world->GetComponent<T>(entity);
        };

        if (name == "Position") {
            auto& comp = getOrAdd.template operator()<PositionComponent>();
            if (value.is<PositionComponent>()) comp = value.as<PositionComponent>();
            else if (value.is<sol::table>()) {
                sol::table t = value.as<sol::table>();
                comp.x = t.get_or("x", comp.x);
                comp.y = t.get_or("y", comp.y);
            }
        }
        else if (name == "Name") {
            auto& comp = getOrAdd.template operator()<NameComponent>();
            if (value.is<NameComponent>()) comp = value.as<NameComponent>();
            else if (value.is<sol::table>()) {
                sol::table t = value.as<sol::table>();
                comp.name = t.get_or("name", comp.name);
            }
            else if (value.is<std::string>()) {
                comp.name = value.as<std::string>();
            }
        }
        else if (name == "Rectangle") {
            auto& comp = getOrAdd.template operator()<RectangleComponent>();
            if (value.is<RectangleComponent>()) comp = value.as<RectangleComponent>();
            else if (value.is<sol::table>()) {
                sol::table t = value.as<sol::table>();
                comp.width = t.get_or("width", comp.width);
                comp.height = t.get_or("height", comp.height);
                comp.layerName = t.get_or("layerName", comp.layerName);
                if (t["background"].valid()) comp.background = ObjectToColor(t["background"]);
                if (t["border"].valid()) comp.border = ObjectToColor(t["border"]);
            }
        }
        else if (name == "Movement") {
            auto& comp = getOrAdd.template operator()<MovementComponent>();
            if (value.is<MovementComponent>()) comp = value.as<MovementComponent>();
            else if (value.is<sol::table>()) {
                sol::table t = value.as<sol::table>();
                comp.speed = t.get_or("speed", comp.speed);
                comp.isMoving = t.get_or("isMoving", comp.isMoving);
                comp.loop = t.get_or("loop", comp.loop);
            }
        }
        else if (name == "Sprite") {
            auto& comp = getOrAdd.template operator()<SpriteComponent>();
            if (value.is<SpriteComponent>()) comp = value.as<SpriteComponent>();
            else if (value.is<sol::table>()) {
                sol::table t = value.as<sol::table>();
                comp.markerName = t.get_or("marker", comp.markerName);
                comp.layerName = t.get_or("layer", comp.layerName);
            }
        }
        else if (name == "Bounds") {
            auto& comp = getOrAdd.template operator()<BoundsComponent>();
            if (value.is<BoundsComponent>()) comp = value.as<BoundsComponent>();
        }
        else if (name == "Team") {
            auto& comp = getOrAdd.template operator()<TeamComponent>();
            if (value.is<TeamComponent>()) comp = value.as<TeamComponent>();
            else if (value.is<sol::table>()) {
                sol::table t = value.as<sol::table>();
                comp.teamId = t.get_or("teamId", comp.teamId);
            }
        }
        else if (name == "Script") {
            auto& comp = getOrAdd.template operator()<ScriptComponent>();
            if (value.is<ScriptComponent>()) comp = value.as<ScriptComponent>();
            else if (value.is<sol::table>()) {
                sol::table t = value.as<sol::table>();
                comp.scriptName = t.get_or("scriptName", comp.scriptName);
                comp.isActive = t.get_or("isActive", comp.isActive);
            }
            else if (value.is<std::string>()) {
                comp.scriptName = value.as<std::string>();
            }
        }
    });

    // AddComponent
    lua.set_function("AddComponent", [](Entity entity, const std::string& name) {
        auto* world = GetActiveWorld();
        if (!world) return;

        if (name == "Position") { if(!world->HasComponent<PositionComponent>(entity)) world->AddComponent<PositionComponent>(entity, {}); }
        else if (name == "Name") { if(!world->HasComponent<NameComponent>(entity)) world->AddComponent<NameComponent>(entity, {}); }
        else if (name == "Rectangle") { if(!world->HasComponent<RectangleComponent>(entity)) world->AddComponent<RectangleComponent>(entity, {}); }
        else if (name == "Movement") { if(!world->HasComponent<MovementComponent>(entity)) world->AddComponent<MovementComponent>(entity, {}); }
        else if (name == "Sprite") { if(!world->HasComponent<SpriteComponent>(entity)) world->AddComponent<SpriteComponent>(entity, {}); }
        else if (name == "Bounds") { if(!world->HasComponent<BoundsComponent>(entity)) world->AddComponent<BoundsComponent>(entity, {}); }
        else if (name == "Team") { if(!world->HasComponent<TeamComponent>(entity)) world->AddComponent<TeamComponent>(entity, {}); }
        else if (name == "Unit") { if(!world->HasComponent<UnitComponent>(entity)) world->AddComponent<UnitComponent>(entity, {}); }
        else if (name == "Script") { if(!world->HasComponent<ScriptComponent>(entity)) world->AddComponent<ScriptComponent>(entity, {}); }
        else {
             LOG_WARNINGF("Lua", "AddComponent: Unknown component type '%s'", name.c_str());
        }
    });

    // HasComponent
    lua.set_function("HasComponent", [](Entity entity, const std::string& name) -> bool {
        auto* world = GetActiveWorld();
        if (!world) return false;
        if (name == "Position") return world->HasComponent<PositionComponent>(entity);
        if (name == "Name") return world->HasComponent<NameComponent>(entity);
        if (name == "Rectangle") return world->HasComponent<RectangleComponent>(entity);
        if (name == "Movement") return world->HasComponent<MovementComponent>(entity);
        if (name == "Sprite") return world->HasComponent<SpriteComponent>(entity);
        if (name == "Bounds") return world->HasComponent<BoundsComponent>(entity);
        if (name == "Team") return world->HasComponent<TeamComponent>(entity);
        if (name == "Unit") return world->HasComponent<UnitComponent>(entity);
        if (name == "Script") return world->HasComponent<ScriptComponent>(entity);
        return false;
    });

    // RemoveComponent
    lua.set_function("RemoveComponent", [](Entity entity, const std::string& name) {
        auto* world = GetActiveWorld();
        if (!world) return;
        if (name == "Position") world->RemoveComponent<PositionComponent>(entity);
        else if (name == "Name") world->RemoveComponent<NameComponent>(entity);
        else if (name == "Rectangle") world->RemoveComponent<RectangleComponent>(entity);
        else if (name == "Movement") world->RemoveComponent<MovementComponent>(entity);
        else if (name == "Sprite") world->RemoveComponent<SpriteComponent>(entity);
        else if (name == "Bounds") world->RemoveComponent<BoundsComponent>(entity);
        else if (name == "Team") world->RemoveComponent<TeamComponent>(entity);
        else if (name == "Unit") world->RemoveComponent<UnitComponent>(entity);
        else if (name == "Script") world->RemoveComponent<ScriptComponent>(entity);
    });

    // GetEntityByName
    lua.set_function("GetEntityByName", [](const std::string& name) -> Entity {
        auto* world = GetActiveWorld();
        if (!world) return 0;
        Entity entity;
        if (world->GetEntityByName(name, &entity)) return entity;
        return 0;
    });
}

void ScriptService::BindRaylibConstants() {
    // Keyboard Keys
    lua["KEY_SPACE"] = 32;
    lua["KEY_ENTER"] = 257;
    lua["KEY_TAB"] = 258;
    lua["KEY_ESCAPE"] = 256;
    lua["KEY_BACKSPACE"] = 259;
    lua["KEY_LEFT"] = 263;
    lua["KEY_RIGHT"] = 262;
    lua["KEY_UP"] = 265;
    lua["KEY_DOWN"] = 264;
    
    for (int i = 0; i < 26; ++i) {
        char name[8];
        snprintf(name, sizeof(name), "KEY_%c", 'A' + i);
        lua[name] = 65 + i;
    }

    for (int i = 0; i < 10; ++i) {
        char name[8];
        snprintf(name, sizeof(name), "KEY_%d", i);
        lua[name] = 48 + i;
    }

    // Mouse Buttons
    lua["MOUSE_LEFT"] = 0;
    lua["MOUSE_RIGHT"] = 1;
    lua["MOUSE_MIDDLE"] = 2;
}

sol::table ScriptService::GetOrLoadScript(const std::string& scriptName) {
    auto it = scriptRegistry.find(scriptName);
    if (it != scriptRegistry.end()) {
        return it->second;
    }

    Path path(scriptName);
    auto result = lua.load_file(path.c_str());
    if (!result.valid()) {
        sol::error err = result;
        LOG_ERRORF("ScriptService", "Failed to load script: %s. Error: %s", scriptName.c_str(), err.what());
        return sol::nil;
    }

    sol::table scriptTable = result();
    if (scriptTable == sol::nil || !scriptTable.is<sol::table>()) {
        LOG_WARNINGF("ScriptService", "Script %s did not return a table.", scriptName.c_str());
        return sol::nil;
    }

    scriptRegistry[scriptName] = scriptTable;
    return scriptTable;
}

sol::table ScriptService::GetEntityInstance(Entity entity, const std::string& scriptName, bool create) {
    auto it = entityScriptInstances.find(entity);
    if (it != entityScriptInstances.end()) {
        return it->second;
    }

    if (!create) return sol::nil;

    sol::table proto = GetOrLoadScript(scriptName);
    if (!proto.valid()) return sol::nil;

    sol::table instance = lua.create_table();
    sol::table mt = lua.create_table();
    mt["__index"] = proto;
    instance[sol::metatable_key] = mt;

    entityScriptInstances[entity] = instance;
    return instance;
}

bool ScriptService::InitEntity(Entity entity, const std::string& scriptName) {
    sol::table instance = GetEntityInstance(entity, scriptName, true);
    if (!instance.valid()) return false;

    sol::function initFunc = instance["init"];
    if (initFunc.valid()) {
        auto result = initFunc(instance, entity);
        if (!result.valid()) {
            sol::error err = result;
            LOG_ERRORF("ScriptService", "Error in %s:init: %s", scriptName.c_str(), err.what());
            return false;
        }
    }
    return true;
}

bool ScriptService::UpdateEntity(Entity entity, const std::string& scriptName, float deltaTime) {
    sol::table instance = GetEntityInstance(entity, scriptName, false);
    if (!instance.valid()) return false;

    sol::function updateFunc = instance["update"];
    if (updateFunc.valid()) {
        auto result = updateFunc(instance, entity, deltaTime);
        if (!result.valid()) {
            sol::error err = result;
            LOG_ERRORF("ScriptService", "Error in %s:update: %s", scriptName.c_str(), err.what());
            return false;
        }
    }
    return true;
}

void ScriptService::OnEntityEvent(Entity entity, const std::string& scriptName, Event& event) {
    sol::table instance = GetEntityInstance(entity, scriptName, false);
    if (!instance.valid()) return;

    sol::function onEventFunc = instance["onEvent"];
    if (!onEventFunc.valid()) return;

    sol::table eventData = lua.create_table();

    auto AddWorldCoords = [&](float screenX, float screenY) {
        Vector2 worldPos = ScreenToWorld({screenX, screenY});
        eventData["wx"] = worldPos.x;
        eventData["wy"] = worldPos.y;
    };

    if (auto* e = event.As<KeyPressedEvent>()) {
        eventData["type"] = "KeyPressed";
        eventData["key"] = e->GetKey();
    }
    else if (auto* e = event.As<KeyReleasedEvent>()) {
        eventData["type"] = "KeyReleased";
        eventData["key"] = e->GetKey();
    }
    else if (auto* e = event.As<MouseButtonPressedEvent>()) {
        eventData["type"] = "MouseButtonPressed";
        eventData["button"] = e->GetButton();
        eventData["x"] = e->GetPosition().x;
        eventData["y"] = e->GetPosition().y;
        AddWorldCoords(e->GetPosition().x, e->GetPosition().y);
    }
    else if (auto* e = event.As<MouseButtonReleasedEvent>()) {
        eventData["type"] = "MouseButtonReleased";
        eventData["button"] = e->GetButton();
        eventData["x"] = e->GetPosition().x;
        eventData["y"] = e->GetPosition().y;
        AddWorldCoords(e->GetPosition().x, e->GetPosition().y);
    }
    else if (auto* e = event.As<MouseMovedEvent>()) {
        eventData["type"] = "MouseMoved";
        eventData["x"] = e->GetPosition().x;
        eventData["y"] = e->GetPosition().y;
        eventData["dx"] = e->GetDelta().x;
        eventData["dy"] = e->GetDelta().y;
        AddWorldCoords(e->GetPosition().x, e->GetPosition().y);
    }
    else if (auto* e = event.As<Elysium::Systems::PickEvent>()) {
        switch (e->type) {
            case Elysium::Systems::PickEvent::Type::PRESS: eventData["type"] = "PickPress"; break;
            case Elysium::Systems::PickEvent::Type::RELEASE: eventData["type"] = "PickRelease"; break;
            case Elysium::Systems::PickEvent::Type::MOVE: eventData["type"] = "PickMove"; break;
        }
        eventData["wx"] = e->position.x;
        eventData["wy"] = e->position.y;
    }

    auto result = onEventFunc(instance, entity, eventData);
    if (!result.valid()) {
        sol::error err = result;
        LOG_ERRORF("ScriptService", "Error in %s:onEvent: %s", scriptName.c_str(), err.what());
    }
}

void ScriptService::ReloadScript(const std::string& scriptName) {
    scriptRegistry.erase(scriptName);
    LOG_INFOF("ScriptService", "Unloaded script: %s", scriptName.c_str());
}

void ScriptService::InspectEntityScript(Entity entity) {
    auto it = entityScriptInstances.find(entity);
    if (it == entityScriptInstances.end()) {
        ImGui::TextDisabled("No script instance.");
        return;
    }

    sol::table instance = it->second;
    for (auto& kv : instance) {
        sol::object key = kv.first;
        sol::object val = kv.second;

        if (!key.is<std::string>()) continue;
        std::string keyStr = key.as<std::string>();
        if (keyStr.empty() || keyStr[0] == '_') continue;

        ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "%s:", keyStr.c_str());
        ImGui::SameLine();

        if (val.is<float>()) ImGui::Text("%.2f", val.as<float>());
        else if (val.is<std::string>()) ImGui::Text("%s", val.as<std::string>().c_str());
        else if (val.is<bool>()) ImGui::Text("%s", val.as<bool>() ? "true" : "false");
        else if (val.is<sol::function>()) ImGui::TextDisabled("(function)");
        else if (val.is<sol::table>()) ImGui::TextDisabled("(table)");
        else ImGui::TextDisabled("?");
    }
}

} // namespace Elysium::Services