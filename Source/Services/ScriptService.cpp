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
    // Raylib types (not ECS components)
    auto vec2 = lua.new_usertype<Vector2>("Vector2", sol::constructors<Vector2(), Vector2(float, float)>());
    vec2["x"] = &Vector2::x;
    vec2["y"] = &Vector2::y;

    auto col = lua.new_usertype<Color>("Color", sol::constructors<Color(), Color(float, float, float, float)>());
    col["r"] = &Color::r;
    col["g"] = &Color::g;
    col["b"] = &Color::b;
    col["a"] = &Color::a;

    auto rect = lua.new_usertype<Rectangle>("Rectangle", sol::constructors<Rectangle()>());
    rect["x"] = &Rectangle::x;
    rect["y"] = &Rectangle::y;
    rect["width"] = &Rectangle::width;
    rect["height"] = &Rectangle::height;

    // Register all ECS components with the registry
    componentRegistry_.Register<PositionComponent>("Position",
        [](sol::usertype<PositionComponent>& ut) {
            ut["x"] = &PositionComponent::x;
            ut["y"] = &PositionComponent::y;
        },
        [](PositionComponent& c, sol::object v) {
            if (v.is<sol::table>()) {
                sol::table t = v.as<sol::table>();
                c.x = t.get_or("x", c.x);
                c.y = t.get_or("y", c.y);
            }
        });

    componentRegistry_.Register<NameComponent>("Name",
        [](sol::usertype<NameComponent>& ut) {
            ut["name"] = &NameComponent::name;
        },
        [](NameComponent& c, sol::object v) {
            if (v.is<sol::table>()) {
                sol::table t = v.as<sol::table>();
                c.name = t.get_or("name", c.name);
            } else if (v.is<std::string>()) {
                c.name = v.as<std::string>();
            }
        });

    componentRegistry_.Register<RectangleComponent>("Rectangle",
        [](sol::usertype<RectangleComponent>& ut) {
            ut["width"] = &RectangleComponent::width;
            ut["height"] = &RectangleComponent::height;
            ut["background"] = sol::property(
                [](RectangleComponent& r) { return r.background; },
                [](RectangleComponent& r, sol::object v) { r.background = ObjectToColor(v); });
            ut["border"] = sol::property(
                [](RectangleComponent& r) { return r.border; },
                [](RectangleComponent& r, sol::object v) { r.border = ObjectToColor(v); });
            ut["layerName"] = &RectangleComponent::layerName;
        },
        [](RectangleComponent& c, sol::object v) {
            if (v.is<sol::table>()) {
                sol::table t = v.as<sol::table>();
                c.width = t.get_or("width", c.width);
                c.height = t.get_or("height", c.height);
                c.layerName = t.get_or("layerName", c.layerName);
                if (t["background"].valid()) c.background = ObjectToColor(t["background"]);
                if (t["border"].valid()) c.border = ObjectToColor(t["border"]);
            }
        });

    componentRegistry_.Register<MovementComponent>("Movement",
        [](sol::usertype<MovementComponent>& ut) {
            ut["speed"] = &MovementComponent::speed;
            ut["isMoving"] = &MovementComponent::isMoving;
            ut["loop"] = &MovementComponent::loop;
            ut["AddWaypoint"] = &MovementComponent::AddWaypoint;
            ut["ClearWaypoints"] = &MovementComponent::ClearWaypoints;
        },
        [](MovementComponent& c, sol::object v) {
            if (v.is<sol::table>()) {
                sol::table t = v.as<sol::table>();
                c.speed = t.get_or("speed", c.speed);
                c.isMoving = t.get_or("isMoving", c.isMoving);
                c.loop = t.get_or("loop", c.loop);
            }
        });

    componentRegistry_.Register<SpriteComponent>("Sprite",
        [](sol::usertype<SpriteComponent>& ut) {
            ut["marker"] = &SpriteComponent::markerName;
            ut["layer"] = &SpriteComponent::layerName;
        },
        [](SpriteComponent& c, sol::object v) {
            if (v.is<sol::table>()) {
                sol::table t = v.as<sol::table>();
                c.markerName = t.get_or("marker", c.markerName);
                c.layerName = t.get_or("layer", c.layerName);
            }
        });

    componentRegistry_.Register<BoundsComponent>("Bounds",
        [](sol::usertype<BoundsComponent>& ut) {
            ut["bounds"] = &BoundsComponent::bounds;
            ut["isDragging"] = &BoundsComponent::isDragging;
            ut["debugColor"] = sol::property(
                [](BoundsComponent& b) { return b.debugColor; },
                [](BoundsComponent& b, sol::object v) { b.debugColor = ObjectToColor(v); });
        },
        nullptr);

    componentRegistry_.Register<TeamComponent>("Team",
        [](sol::usertype<TeamComponent>& ut) {
            ut["teamId"] = &TeamComponent::teamId;
        },
        [](TeamComponent& c, sol::object v) {
            if (v.is<sol::table>()) {
                sol::table t = v.as<sol::table>();
                c.teamId = t.get_or("teamId", c.teamId);
            }
        });

    componentRegistry_.Register<UnitComponent>("Unit",
        [](sol::usertype<UnitComponent>& ut) {
            ut["hasActedThisTurn"] = &UnitComponent::hasActedThisTurn;
            ut["canMove"] = &UnitComponent::canMove;
            ut["canAttack"] = &UnitComponent::canAttack;
            ut["canCastSpells"] = &UnitComponent::canCastSpells;
            ut["canUseItems"] = &UnitComponent::canUseItems;
        },
        [](UnitComponent& c, sol::object v) {
            if (v.is<sol::table>()) {
                sol::table t = v.as<sol::table>();
                c.hasActedThisTurn = t.get_or("hasActedThisTurn", c.hasActedThisTurn);
                c.canMove = t.get_or("canMove", c.canMove);
                c.canAttack = t.get_or("canAttack", c.canAttack);
                c.canCastSpells = t.get_or("canCastSpells", c.canCastSpells);
                c.canUseItems = t.get_or("canUseItems", c.canUseItems);
            }
        });

    componentRegistry_.Register<ScriptComponent>("Script",
        [](sol::usertype<ScriptComponent>& ut) {
            ut["scriptName"] = &ScriptComponent::scriptName;
            ut["isActive"] = &ScriptComponent::isActive;
            ut["isInitialized"] = &ScriptComponent::isInitialized;
        },
        [](ScriptComponent& c, sol::object v) {
            if (v.is<sol::table>()) {
                sol::table t = v.as<sol::table>();
                c.scriptName = t.get_or("scriptName", c.scriptName);
                c.isActive = t.get_or("isActive", c.isActive);
            } else if (v.is<std::string>()) {
                c.scriptName = v.as<std::string>();
            }
        });

    componentRegistry_.Register<HealthComponent>("Health",
        [](sol::usertype<HealthComponent>& ut) {
            ut["current"] = &HealthComponent::current;
            ut["max"] = &HealthComponent::max;
        },
        [](HealthComponent& c, sol::object v) {
            if (v.is<sol::table>()) {
                sol::table t = v.as<sol::table>();
                c.current = t.get_or("current", c.current);
                c.max = t.get_or("max", c.max);
            }
        });

    componentRegistry_.Register<FactionComponent>("Faction",
        [](sol::usertype<FactionComponent>& ut) {
            ut["name"] = &FactionComponent::name;
        },
        [](FactionComponent& c, sol::object v) {
            if (v.is<sol::table>()) {
                sol::table t = v.as<sol::table>();
                c.name = t.get_or("name", c.name);
            }
        });

    componentRegistry_.Register<KinematicsComponent>("Kinematics",
        [](sol::usertype<KinematicsComponent>& ut) {
            ut["maxSpeed"] = &KinematicsComponent::maxSpeed;
            ut["friction"] = &KinematicsComponent::friction;
            ut["velocity"] = &KinematicsComponent::velocity;
            ut["acceleration"] = &KinematicsComponent::acceleration;
        },
        [](KinematicsComponent& c, sol::object v) {
            if (v.is<sol::table>()) {
                sol::table t = v.as<sol::table>();
                c.maxSpeed = t.get_or("maxSpeed", c.maxSpeed);
                c.friction = t.get_or("friction", c.friction);
                if (t["velocity"].valid()) {
                    sol::table vt = t["velocity"];
                    c.velocity.x = vt.get_or("x", c.velocity.x);
                    c.velocity.y = vt.get_or("y", c.velocity.y);
                }
                if (t["acceleration"].valid()) {
                    sol::table at = t["acceleration"];
                    c.acceleration.x = at.get_or("x", c.acceleration.x);
                    c.acceleration.y = at.get_or("y", c.acceleration.y);
                }
            }
        });

    componentRegistry_.Register<AttackComponent>("Attack",
        [](sol::usertype<AttackComponent>& ut) {
            ut["damage"] = &AttackComponent::damage;
            ut["range"] = &AttackComponent::range;
            ut["cooldown"] = &AttackComponent::cooldown;
            ut["timer"] = &AttackComponent::timer;
            ut["isAttacking"] = &AttackComponent::isAttacking;
            ut["targetId"] = &AttackComponent::targetId;
        },
        [](AttackComponent& c, sol::object v) {
            if (v.is<sol::table>()) {
                sol::table t = v.as<sol::table>();
                c.damage = t.get_or("damage", c.damage);
                c.range = t.get_or("range", c.range);
                c.cooldown = t.get_or("cooldown", c.cooldown);
                c.timer = t.get_or("timer", c.timer);
                c.isAttacking = t.get_or("isAttacking", c.isAttacking);
                c.targetId = t.get_or("targetId", c.targetId);
            }
        });

    // Register all usertypes with Lua
    componentRegistry_.RegisterAllUserTypes(lua);
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
        return componentRegistry_.GetComponent(name, lua, world, entity);
    });

    // SetComponent
    lua.set_function("SetComponent", [this](Entity entity, const std::string& name, sol::object value) {
        auto* world = GetActiveWorld();
        if (!world) return;
        componentRegistry_.SetComponent(name, world, entity, value);
    });

    // AddComponent
    lua.set_function("AddComponent", [this](Entity entity, const std::string& name) {
        auto* world = GetActiveWorld();
        if (!world) return;
        componentRegistry_.Add(name, world, entity);
    });

    // HasComponent
    lua.set_function("HasComponent", [this](Entity entity, const std::string& name) -> bool {
        auto* world = GetActiveWorld();
        if (!world) return false;
        return componentRegistry_.Has(name, world, entity);
    });

    // RemoveComponent
    lua.set_function("RemoveComponent", [this](Entity entity, const std::string& name) {
        auto* world = GetActiveWorld();
        if (!world) return;
        componentRegistry_.Remove(name, world, entity);
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