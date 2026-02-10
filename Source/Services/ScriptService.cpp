#define SOL_HEADER_ONLY 1
#define SOL_ALL_SAFETIES_ON 1
#include "Services/ScriptService.h"
#include "Core/Application.h"
#include "Services/LogService.h"
#include "Services/SceneService.h"
#include "Services/AssetService.h"
#include "Core/Scene.h"
#include "Core/Component.h"
#include "Core/ComponentRegistry.h"
#include "Core/Path.h"
#include "imgui.h"
#include <memory>
#include <limits>
#include <cmath>
#include "Components/CameraComponent.h"
#include "Components/PositionComponent.h"
#include "Systems/CollisionSystem.h"


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

sol::protected_function_result ScriptService::ExecuteString(const std::string& scriptString) {
    auto result = lua.safe_script(scriptString, sol::script_pass_on_error);
    if (!result.valid()) {
        sol::error err = result;
        LOG_ERRORF("ScriptService", "Lua Error: %s", err.what());
        return result;
    }

    return result;
}

void ScriptService::InitLuaContext() {
    lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::table, sol::lib::string, sol::lib::math, sol::lib::debug);

    // Configure package.path so require("Scripts/Component") resolves to Assets/Scripts/Component.lua
    std::string assetsPath = ASSETS_PATH;
    lua["package"]["path"] = assetsPath + "?.lua;" + assetsPath + "?/init.lua";
}

// Active world set by ScriptSystem before executing scripts
static Elysium::World* s_activeWorld = nullptr;

void ScriptService::SetActiveWorld(Elysium::World* w) {
    s_activeWorld = w;
}

static Elysium::World* GetActiveWorld() {
    if (s_activeWorld) return s_activeWorld;
    // Fallback for scripts run outside ScriptSystem (e.g. editor Lua filter)
    auto& app = Elysium::Application::GetInstance();
    auto* scene = app.GetService<Elysium::Services::SceneService>().GetScene();
    return scene ? scene->GetWorld() : nullptr;
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

    // Bind Global Registry Components
    Elysium::ComponentRegistry::Instance().BindAllScripts(lua);
}

void ScriptService::BindEntityAPI() {
    // Log
    lua.set_function("Log", [](const std::string& msg) {
        LOG_INFO("Lua", msg.c_str());
    });

    lua.set_function("GetEntities", [](sol::this_state s) -> sol::table {
        sol::state_view lua(s); // Get a view of the current state
        sol::table result = lua.create_table();
        
        auto* world = GetActiveWorld();
        if (!world) return result;

        int index = 1;
        // Ensure world->GetLivingEntities() returns a container compatible with range-based for
        for (Entity e : world->GetLivingEntities()) {
            result[index++] = e;
        }
        return result;
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
        
        if (auto* access = Elysium::ComponentRegistry::Instance().GetLuaAccess(name)) {
            return access->get(world, entity, lua);
        }
        return sol::nil;
    });

    // SetComponent
    lua.set_function("SetComponent", [this](Entity entity, const std::string& name, sol::object value) {
        auto* world = GetActiveWorld();
        if (!world) return;
        
        if (auto* access = Elysium::ComponentRegistry::Instance().GetLuaAccess(name)) {
            access->set(world, entity, value);
            return;
        }
    });

    // AddComponent
    lua.set_function("AddComponent", [this](Entity entity, const std::string& name) {
        auto* world = GetActiveWorld();
        if (!world) return;

        if (auto* access = Elysium::ComponentRegistry::Instance().GetLuaAccess(name)) {
            access->add(world, entity);
            return;
        }
    });

    // HasComponent
    lua.set_function("HasComponent", [this](Entity entity, const std::string& name) -> bool {
        auto* world = GetActiveWorld();
        if (!world) return false;

        if (auto* access = Elysium::ComponentRegistry::Instance().GetLuaAccess(name)) {
            return access->has(world, entity);
        }
        return false;
    });

    // RemoveComponent
    lua.set_function("RemoveComponent", [this](Entity entity, const std::string& name) {
        auto* world = GetActiveWorld();
        if (!world) return;

        if (auto* access = Elysium::ComponentRegistry::Instance().GetLuaAccess(name)) {
            access->remove(world, entity);
            return;
        }
    });

    // GetEntityByName
    lua.set_function("GetEntityByName", [](const std::string& name) -> Entity {
        auto* world = GetActiveWorld();
        if (!world) return 0;
        Entity entity;
        if (world->GetEntityByName(name, &entity)) return entity;
        return 0;
    });

    // FindEntitiesWithComponent - returns table of entities with given component
    lua.set_function("FindEntitiesWithComponent", [this](const std::string& componentName) -> sol::table {
        sol::table result = lua.create_table();
        auto* world = GetActiveWorld();
        if (!world) return result;

        int index = 1;
        for (Entity e : world->GetLivingEntities()) {
            bool has = false;
            
            if (auto* access = Elysium::ComponentRegistry::Instance().GetLuaAccess(componentName)) {
                has = access->has(world, e);
            }

            if (has) {
                result[index++] = e;
            }
        }
        return result;
    });

    // FindNearestEntity - finds entity with component nearest to position
    lua.set_function("FindNearestEntity", [this](float x, float y, const std::string& componentName) -> Entity {
        auto* world = GetActiveWorld();
        if (!world) return 0;

        Entity nearest = 0;
        float nearestDist = std::numeric_limits<float>::max();

        for (Entity e : world->GetLivingEntities()) {
            bool has = false;
             if (auto* access = Elysium::ComponentRegistry::Instance().GetLuaAccess(componentName)) {
                has = access->has(world, e);
            }
            if (!has) continue;

            if (!world->HasComponent<PositionComponent>(e)) continue;

            auto& pos = world->GetComponent<PositionComponent>(e);
            float dx = pos.x - x;
            float dy = pos.y - y;
            float dist = dx * dx + dy * dy;

            if (dist < nearestDist) {
                nearestDist = dist;
                nearest = e;
            }
        }
        return nearest;
    });

    // Distance - compute distance between two points
    lua.set_function("Distance", [](float x1, float y1, float x2, float y2) -> float {
        float dx = x2 - x1;
        float dy = y2 - y1;
        return std::sqrt(dx * dx + dy * dy);
    });

    // Collision queries
    lua.set_function("AreColliding", [](Entity a, Entity b) -> bool {
        auto& app = Elysium::Application::GetInstance();
        auto* scene = app.GetService<Elysium::Services::SceneService>().GetScene();
        if (!scene) return false;

        auto* collisionSystem = scene->GetSystem<Elysium::Systems::CollisionSystem>();
        if (!collisionSystem) return false;

        return collisionSystem->AreColliding(a, b);
    });

    lua.set_function("GetCollisions", [this](Entity entity) -> sol::table {
        sol::table result = lua.create_table();

        auto& app = Elysium::Application::GetInstance();
        auto* scene = app.GetService<Elysium::Services::SceneService>().GetScene();
        if (!scene) return result;

        auto* collisionSystem = scene->GetSystem<Elysium::Systems::CollisionSystem>();
        if (!collisionSystem) return result;

        auto collisions = collisionSystem->GetCollisionsWith(entity);
        int index = 1;
        for (Entity e : collisions) {
            result[index++] = e;
        }
        return result;
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

sol::table ScriptService::GetOrLoadScript(Path path) {
    auto it = scriptRegistry.find(path);
    if (it != scriptRegistry.end()) {
        return it->second;
    }

    auto& assetService = Elysium::Application::GetInstance().GetService<Elysium::Services::AssetService>();
    auto asset = assetService.GetAsset(path);
    if (!asset) {
        LOG_ERRORF("ScriptService", "Failed to load script: %s. Error: Asset not found", path.c_str());
        return sol::nil;
    }

    auto script= asset->GetScript();
    if (script.source.empty()) {
        LOG_ERRORF("ScriptService", "Failed to load script: %s. Error: Script is empty", path.c_str());
        return sol::nil;
    }

    auto result = lua.load(script.source);
    if (!result.valid()) {
        sol::error err = result;
        LOG_ERRORF("ScriptService", "Failed to load script: %s. Error: %s", path.c_str(), err.what());
        return sol::nil;
    }

    sol::table scriptTable = result();
    if (scriptTable == sol::nil || !scriptTable.is<sol::table>()) {
        LOG_WARNINGF("ScriptService", "Script %s did not return a table.", path.c_str());
        return sol::nil;
    }

    scriptRegistry[path] = scriptTable;
    return scriptTable;
}

sol::table ScriptService::GetEntityInstance(Entity entity, Path scriptPath, bool create) {
    auto entityIt = entityScriptInstances.find(entity);
    if (entityIt != entityScriptInstances.end()) {
        auto scriptIt = entityIt->second.find(scriptPath);
        if (scriptIt != entityIt->second.end()) {
            return scriptIt->second;
        }
    }

    if (!create) return sol::nil;

    sol::table proto = GetOrLoadScript(scriptPath);
    if (!proto.valid()) return sol::nil;

    sol::table instance = lua.create_table();
    sol::table mt = lua.create_table();
    mt["__index"] = proto;
    instance[sol::metatable_key] = mt;

    entityScriptInstances[entity][scriptPath] = instance;
    return instance;
}

bool ScriptService::InitializeEntity(Entity entity, Path scriptName) {
    sol::table instance = GetEntityInstance(entity, scriptName, true);
    if (!instance.valid()) return false;

    sol::function initFunc = instance["Initialize"];
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

bool ScriptService::UpdateEntity(Entity entity, Path scriptName, float deltaTime) {
    sol::table instance = GetEntityInstance(entity, scriptName, false);
    if (!instance.valid()) return false;

    sol::function updateFunc = instance["Update"];
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

void ScriptService::OnEntityEvent(Entity entity, Path scriptPath, Event& event) {
    sol::table instance = GetEntityInstance(entity, scriptPath, false);
    if (!instance.valid()) return;

    sol::function onEventFunc = instance["OnEvent"];
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

    auto result = onEventFunc(instance, entity, eventData);
    if (!result.valid()) {
        sol::error err = result;
        LOG_ERRORF("ScriptService", "Error in %s:onEvent: %s", scriptPath.c_str(), err.what());
    }
}

void ScriptService::ReloadScript(Path scriptPath) {
    scriptRegistry.erase(scriptPath);
    LOG_INFOF("ScriptService", "Unloaded script: %s", scriptPath.c_str());
}

void ScriptService::InspectEntityScript(Entity entity, Path scriptPath) {
    auto entityIt = entityScriptInstances.find(entity);
    if (entityIt == entityScriptInstances.end()) {
        ImGui::TextDisabled("No script instance.");
        return;
    }

    auto scriptIt = entityIt->second.find(scriptPath);
    if (scriptIt == entityIt->second.end()) {
        ImGui::TextDisabled("No script instance.");
        return;
    }

    sol::table instance = scriptIt->second;

    // Track which fields we've seen to detect new ones
    static std::unordered_map<Entity, std::unordered_set<std::string>> seenFields;
    auto& seen = seenFields[entity];

    for (auto& kv : instance) {
        sol::object key = kv.first;
        sol::object val = kv.second;

        if (!key.is<std::string>())
            continue;
        std::string keyStr = key.as<std::string>();
        if (keyStr.empty() || keyStr[0] == '_')
            continue;

        seen.insert(keyStr);

        // Skip functions and tables for now
        if (val.is<sol::function>() || val.is<sol::table>()) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s:", keyStr.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled(val.is<sol::function>() ? "(function)" : "(table)");
            continue;
        }

        ImGui::PushID(keyStr.c_str());

        // Editable fields
        if (val.is<float>() || val.is<double>()) {
            float value = val.as<float>();
            ImGui::SetNextItemWidth(150);
            if (ImGui::DragFloat(keyStr.c_str(), &value, 0.1f)) {
                instance[keyStr] = value;
            }
        } else if (val.is<int>()) {
            int value = val.as<int>();
            ImGui::SetNextItemWidth(150);
            if (ImGui::DragInt(keyStr.c_str(), &value)) {
                instance[keyStr] = value;
            }
        } else if (val.is<bool>()) {
            bool value = val.as<bool>();
            if (ImGui::Checkbox(keyStr.c_str(), &value)) {
                instance[keyStr] = value;
            }
        } else if (val.is<std::string>()) {
            std::string value = val.as<std::string>();
            char buffer[256];
            strncpy(buffer, value.c_str(), sizeof(buffer) - 1);
            buffer[sizeof(buffer) - 1] = '\0';

            ImGui::SetNextItemWidth(200);
            if (ImGui::InputText(keyStr.c_str(), buffer, sizeof(buffer))) {
                instance[keyStr] = std::string(buffer);
            }
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s:", keyStr.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("(unknown type)");
        }

        ImGui::PopID();
    }

    // Optional: Add button to add new fields
    ImGui::Separator();
    if (ImGui::Button("+ Add Field")) {
        ImGui::OpenPopup("AddFieldPopup");
    }

    if (ImGui::BeginPopup("AddFieldPopup")) {
        static char fieldName[64] = "";
        static int fieldType = 0;  // 0=float, 1=int, 2=bool, 3=string
        static char fieldValue[256] = "";

        ImGui::InputText("Name", fieldName, sizeof(fieldName));
        ImGui::Combo("Type", &fieldType, "Float\0Int\0Bool\0String\0");

        if (fieldType != 2) {  // Not bool
            ImGui::InputText("Value", fieldValue, sizeof(fieldValue));
        } else {
            static bool boolValue = false;
            ImGui::Checkbox("Value", &boolValue);
            strcpy(fieldValue, boolValue ? "true" : "false");
        }

        if (ImGui::Button("Add")) {
            if (strlen(fieldName) > 0) {
                switch (fieldType) {
                    case 0:
                        instance[fieldName] = (float)atof(fieldValue);
                        break;
                    case 1:
                        instance[fieldName] = atoi(fieldValue);
                        break;
                    case 2:
                        instance[fieldName] = (strcmp(fieldValue, "true") == 0);
                        break;
                    case 3:
                        instance[fieldName] = std::string(fieldValue);
                        break;
                }
                fieldName[0] = '\0';
                fieldValue[0] = '\0';
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

} // namespace Elysium::Services
