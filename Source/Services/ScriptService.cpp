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
    lua.new_usertype<Vector2>("Vector2",
        sol::constructors<Vector2(), Vector2(float, float)>(),
        "x", &Vector2::x,
        "y", &Vector2::y
    );

    // Color (Raylib)
    lua.new_usertype<Color>("Color",
        "r", &Color::r,
        "g", &Color::g,
        "b", &Color::b,
        "a", &Color::a
    );

    // Rectangle (Raylib)
    lua.new_usertype<Rectangle>("Rectangle",
        "x", &Rectangle::x,
        "y", &Rectangle::y,
        "width", &Rectangle::width,
        "height", &Rectangle::height
    );

    // PositionComponent
    lua.new_usertype<PositionComponent>("PositionComponent",
        sol::constructors<PositionComponent(), PositionComponent(float, float)>(),
        "x", &PositionComponent::x,
        "y", &PositionComponent::y
    );

    // RectangleComponent
    lua.new_usertype<RectangleComponent>("RectangleComponent",
        sol::constructors<RectangleComponent(), RectangleComponent(float, float, Color, Color, const std::string&)>(),
        "width", &RectangleComponent::width,
        "height", &RectangleComponent::height,
        "background", sol::property([](RectangleComponent& r) { return r.background; }, [](RectangleComponent& r, sol::object v) { r.background = ObjectToColor(v); }),
        "border", sol::property([](RectangleComponent& r) { return r.border; }, [](RectangleComponent& r, sol::object v) { r.border = ObjectToColor(v); }),
        "layerName", &RectangleComponent::layerName
    );

    // MovementComponent
    lua.new_usertype<MovementComponent>("MovementComponent",
        sol::constructors<MovementComponent()>(),
        "speed", &MovementComponent::speed,
        "isMoving", &MovementComponent::isMoving,
        "loop", &MovementComponent::loop,
        "AddWaypoint", &MovementComponent::AddWaypoint,
        "ClearWaypoints", &MovementComponent::ClearWaypoints
    );

    // SpriteComponent
    lua.new_usertype<SpriteComponent>("SpriteComponent",
        sol::constructors<SpriteComponent()>(),
        "marker", &SpriteComponent::markerName,
        "layer", &SpriteComponent::layerName
    );

    // BoundsComponent
    lua.new_usertype<BoundsComponent>("BoundsComponent",
        sol::constructors<BoundsComponent(), BoundsComponent(Rectangle, Color)>(),
        "bounds", &BoundsComponent::bounds,
        "isDragging", &BoundsComponent::isDragging,
        "debugColor", sol::property([](BoundsComponent& b) { return b.debugColor; }, [](BoundsComponent& b, sol::object v) { b.debugColor = ObjectToColor(v); })
    );

    // TeamComponent
    lua.new_usertype<TeamComponent>("TeamComponent",
        sol::constructors<TeamComponent(), TeamComponent(int)>(),
        "teamId", &TeamComponent::teamId
    );

    // UnitComponent
    lua.new_usertype<UnitComponent>("UnitComponent",
        sol::constructors<UnitComponent()>(),
        "hasActedThisTurn", &UnitComponent::hasActedThisTurn,
        "canMove", &UnitComponent::canMove,
        "canAttack", &UnitComponent::canAttack
    );
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
        LOG_INFOF("Lua", "Mouse Position: %f, %f", m.x, m.y);
        auto output = ScreenToWorld(m);
        LOG_INFOF("Lua", "Converted World Position: %f, %f", output.x, output.y);
        return output;
    });

    // GetComponent
    lua.set_function("GetComponent", [this](Entity entity, const std::string& name) -> sol::object {
        auto* world = GetActiveWorld();
        if (!world) return sol::nil;

        if (name == "Position") {
            if (world->HasComponent<PositionComponent>(entity))
                return sol::make_object(lua.lua_state(), &world->GetComponent<PositionComponent>(entity));
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
    });

    // AddComponent
    lua.set_function("AddComponent", [](Entity entity, const std::string& name) {
        auto* world = GetActiveWorld();
        if (!world) return;

        if (name == "Position") { if(!world->HasComponent<PositionComponent>(entity)) world->AddComponent<PositionComponent>(entity, {}); }
        else if (name == "Rectangle") { if(!world->HasComponent<RectangleComponent>(entity)) world->AddComponent<RectangleComponent>(entity, {}); }
        else if (name == "Movement") { if(!world->HasComponent<MovementComponent>(entity)) world->AddComponent<MovementComponent>(entity, {}); }
        else if (name == "Sprite") { if(!world->HasComponent<SpriteComponent>(entity)) world->AddComponent<SpriteComponent>(entity, {}); }
        else if (name == "Bounds") { if(!world->HasComponent<BoundsComponent>(entity)) world->AddComponent<BoundsComponent>(entity, {}); }
        else if (name == "Team") { if(!world->HasComponent<TeamComponent>(entity)) world->AddComponent<TeamComponent>(entity, {}); }
        else if (name == "Unit") { if(!world->HasComponent<UnitComponent>(entity)) world->AddComponent<UnitComponent>(entity, {}); }
        else {
             LOG_WARNINGF("Lua", "AddComponent: Unknown component type '%s'", name.c_str());
        }
    });

    // HasComponent
    lua.set_function("HasComponent", [](Entity entity, const std::string& name) -> bool {
        auto* world = GetActiveWorld();
        if (!world) return false;
        if (name == "Position") return world->HasComponent<PositionComponent>(entity);
        if (name == "Rectangle") return world->HasComponent<RectangleComponent>(entity);
        if (name == "Movement") return world->HasComponent<MovementComponent>(entity);
        if (name == "Sprite") return world->HasComponent<SpriteComponent>(entity);
        if (name == "Bounds") return world->HasComponent<BoundsComponent>(entity);
        if (name == "Team") return world->HasComponent<TeamComponent>(entity);
        if (name == "Unit") return world->HasComponent<UnitComponent>(entity);
        return false;
    });

    // RemoveComponent
    lua.set_function("RemoveComponent", [](Entity entity, const std::string& name) {
        auto* world = GetActiveWorld();
        if (!world) return;
        if (name == "Position") world->RemoveComponent<PositionComponent>(entity);
        else if (name == "Rectangle") world->RemoveComponent<RectangleComponent>(entity);
        else if (name == "Movement") world->RemoveComponent<MovementComponent>(entity);
        else if (name == "Sprite") world->RemoveComponent<SpriteComponent>(entity);
        else if (name == "Bounds") world->RemoveComponent<BoundsComponent>(entity);
        else if (name == "Team") world->RemoveComponent<TeamComponent>(entity);
        else if (name == "Unit") world->RemoveComponent<UnitComponent>(entity);
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