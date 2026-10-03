#define SOL_HEADER_ONLY 1
#define SOL_ALL_SAFETIES_ON 1
#include "Services/ScriptService.h"
#include "Core/Common.h"
#include "Core/Script.h"
#include "Interfaces/IAssetService.h"
#include "Interfaces/ISceneService.h"
#include "Services/LogService.h"
#include "Services/SceneService.h"
#include "Services/AssetService.h"
#include "Core/Scene.h"
#include "Core/Component.h"
#include "Core/ComponentRegistry.h"
#include "Core/Path.h"
#include "Core/Prefab.h"
#include "Core/Input.h"
#include <memory>
#include <limits>
#include <cmath>
#include "Core/Components/CameraComponent.h"
#include "Core/Components/TransformComponent.h"
#include "Core/Systems/CollisionSystem.h"
#include "Core/Systems/MovementSystem.h"
#include "Core/Systems/NavMeshSystem.h"
#include "Core/Systems/RenderSystem.h"
#include <algorithm>
#include <optional>
#include <tuple>


namespace Elysium::Services {

// Lua bindings below are captureless free functions with no instance context,
// same reason s_activeWorld exists — this mirrors that existing pattern
// instead of introducing a new one.
static ServiceLocator* s_services = nullptr;

ScriptService::ScriptService(ServiceLocator& registry) {
    s_services = &registry;
}

ScriptService::~ScriptService() {
}

void ScriptService::Initialize() {
    InitLuaContext();
    BindComponents();
    BindEntityAPI();
    BindInputConstants();
    LOG_INFO("ScriptService", "Lua VM Initialized with sol2 and component usertypes");
}

void ScriptService::Shutdown() {
    LOG_INFO("ScriptService", "Lua VM Shutdown");
}

void ScriptService::Update(float deltaTime) {
}

Elysium::ScriptResult ScriptService::ExecuteString(const std::string& scriptString) {
    ProfileN("ScriptService ExecuteString");
    auto result = lua.safe_script(scriptString, sol::script_pass_on_error);
    if (!result.valid()) {
        sol::error err = result;
        LOG_ERRORF("ScriptService", "Lua Error: %s", err.what());
        return Elysium::ScriptResult{false, err.what()};
    }

    return Elysium::ScriptResult{true, {}};
}

std::vector<Entity> ScriptService::FilterEntities(const std::string& filterFunctionBody) {
    ProfileN("ScriptService FilterEntities");

    // Wraps the user's `function filter(e) -> bool` to collect matching entities.
    const char* driverTemplate = R"(
        %s

        local entities = GetEntities()
        local result = {}
        for i, e in ipairs(entities) do
            if filter(e) then
                table.insert(result, e)
            end
        end

        return result
    )";

    char finalScript[2048];
    snprintf(finalScript, sizeof(finalScript), driverTemplate, filterFunctionBody.c_str());

    std::vector<Entity> matches;

    auto result = lua.safe_script(finalScript, sol::script_pass_on_error);
    if (!result.valid()) {
        sol::error err = result;
        LOG_ERRORF("ScriptService", "Lua Error in filter script: %s", err.what());
        return matches;
    }

    sol::table entityTable = result;
    for (auto& kv : entityTable) {
        sol::object val = kv.second;
        if (val.is<Entity>()) {
            matches.push_back(val.as<Entity>());
        }
    }

    return matches;
}

void ScriptService::InitLuaContext() {
    lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::table, sol::lib::string, sol::lib::math, sol::lib::coroutine, sol::lib::debug);

    // Configure package.path so require("Scripts/Foo") resolves Lua modules two ways:
    // first against the current project's own asset root (game scripts), falling
    // back to the engine's compile-time ASSETS_PATH (shared Lua helpers like
    // Scripts/Elysium/Component.lua that ship with the engine, not the project).
    std::string projectAssetsPath = Path::GetAssetsRoot();
    std::string engineAssetsPath = ASSETS_PATH;
    lua["package"]["path"] = projectAssetsPath + "?.lua;" + projectAssetsPath + "?/init.lua;" +
                              engineAssetsPath + "?.lua;" + engineAssetsPath + "?/init.lua";
}

// Active world set by ScriptSystem before executing scripts
static Elysium::World* s_activeWorld = nullptr;

void ScriptService::SetActiveWorld(World* w) {
    s_activeWorld = w;
}

static Elysium::World* GetActiveWorld() {
    if (s_activeWorld) return s_activeWorld;
    // Fallback for scripts run outside ScriptSystem (e.g. editor Lua filter)
    auto* scene = s_services->Get<ISceneService>().GetTopScene();
    return scene ? scene->GetWorld() : nullptr;
}

// RenderSystem is per-Scene, not a Service — reach it via the top scene, same as AreColliding/IssueMoveCommand.
static Elysium::Systems::RenderSystem* GetCurrentRenderSystem() {
    auto* scene = s_services->Get<ISceneService>().GetTopScene();
    return scene ? scene->GetSystem<Elysium::Systems::RenderSystem>() : nullptr;
}

// The first visible camera's view (orbit included), or nothing.
static std::optional<Elysium::Systems::CameraView> ActiveCameraView() {
    auto* world = GetActiveWorld();
    if (!world) return std::nullopt;
    std::optional<Elysium::Systems::CameraView> view;
    world->Query<CameraComponent>([&](Entity camEnt, auto& cameraComp) {
        if (view || !cameraComp.isVisible) return;
        Vector2 position = {0, 0};
        if (world->HasComponent<TransformComponent>(camEnt)) {
            auto& transform = world->GetComponent<TransformComponent>(camEnt);
            position = {transform.worldX, transform.worldY};
        }
        Elysium::Systems::CameraView v{position, cameraComp.zoom != 0.0f ? cameraComp.zoom : 1.0f, cameraComp.viewport};
        v.yaw = cameraComp.yaw;
        v.pitch = std::clamp(cameraComp.pitch, 5.0f, 89.0f);
        view = v;
    });
    return view;
}

static Vector2 WorldToScreen(const Vector2& worldPos) {
    auto view = ActiveCameraView();
    return view ? Elysium::Systems::RenderProjector::WorldToFramebuffer(worldPos, *view) : worldPos;
}

static Vector2 ScreenToWorld(Vector2 screenPos) {
    auto view = ActiveCameraView();
    return view ? Elysium::Systems::RenderProjector::FramebufferToWorld(screenPos, *view) : screenPos;
}

// Where ground layers draw the 3D point (x, y) at height z: the ground-plane point on the same
// view ray. The default camera's is (x, y - z cos 30).
static Vector2 ViewLift(float x, float y, float z) {
    auto view = ActiveCameraView();
    if (!view || view->IsDefaultOrientation()) return {x, y - z * Elysium::World3D::kPitchCos};
    const Elysium::World3D::View v = Elysium::Systems::RenderProjector::View3D(*view);
    return v.FramebufferToGround(v.WorldToFramebuffer(x, y, z));
}

// The screen pixel of the 3D point (x, y) at height z.
static Vector2 ViewProject(float x, float y, float z) {
    auto view = ActiveCameraView();
    if (!view) return {x, y - z * Elysium::World3D::kPitchCos};
    return Elysium::Systems::RenderProjector::View3D(*view).WorldToFramebuffer(x, y, z);
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

    // SpawnPrefab(path [, x, y, z]): spawns a project-relative prefab, its root placed at
    // (x, y, z). Returns the root entity, or nil.
    lua.set_function("SpawnPrefab", [this](const std::string& path, sol::optional<float> x, sol::optional<float> y,
                                           sol::optional<float> z) -> sol::object {
        auto* world = GetActiveWorld();
        if (!world) return sol::nil;
        const Prefab* prefab = Prefab::Get(s_services->Get<IAssetService>(), Path(path).GetFullPath());
        if (!prefab) return sol::nil;
        static int spawnCount = 0;
        PrefabSpawnResult result = prefab->Spawn(world, "Spawn" + std::to_string(++spawnCount), *s_services);
        if (result.spawned.empty()) return sol::nil;
        const Entity root = result.ids.count(0) ? result.ids.at(0) : result.spawned.front();
        if (world->HasComponent<TransformComponent>(root)) {
            auto& t = world->GetComponent<TransformComponent>(root);
            t.localX = x.value_or(t.localX);
            t.localY = y.value_or(t.localY);
            t.localZ = z.value_or(t.localZ);
        }
        return sol::make_object(lua, root);
    });

    // Random
    lua.set_function("Random", [](int min, int max) {
        return min + (std::rand() % (max - min + 1));
    });

    // Scene
    lua.set_function("SceneClear", []() {
        s_services->Get<ISceneService>().Clear();
    });
    lua.set_function("SceneReplace", [](const std::string& sceneName) {
        s_services->Get<ISceneService>().Replace(sceneName);
    });
    lua.set_function("ScenePush", [](const std::string& sceneName) {
        s_services->Get<ISceneService>().Push(sceneName);
    });
    lua.set_function("ScenePop", []() {
        s_services->Get<ISceneService>().Pop();
    });

    // Input Polling
    lua.set_function("IsKeyDown", [](int key) { return Input::IsKeyDown(static_cast<Key>(key)); });
    lua.set_function("IsKeyPressed", [](int key) { return Input::IsKeyPressed(static_cast<Key>(key)); });
    lua.set_function("IsMouseButtonDown", [](int button) { return Input::IsMouseButtonDown(static_cast<MouseButton>(button)); });
    lua.set_function("IsMouseButtonPressed", [](int button) { return Input::IsMouseButtonPressed(static_cast<MouseButton>(button)); });
    lua.set_function("GetMouseWheelMove", []() { return Input::GetMouseWheelMove(); });
    lua.set_function("GetMousePosition", [this]() {
        Vector2 m = this->_mousePosition; // Cached by SceneService from Input polling each frame
        return m;
        // return ScreenToWorld(m);
    });

    lua.set_function("WorldToScreen", [](const Vector2& worldPos) {
        Vector2 screenPos = WorldToScreen(worldPos);
        return screenPos;
    });

    lua.set_function("ScreenToWorld", [](const Vector2& screenPos) {
        Vector2 worldPos = ScreenToWorld(screenPos);
        return worldPos;
    });
    // ViewLift(x, y, z) -> x, y: where ground layers ("overlay", "fx") draw the point (x, y) at
    // height z under the current camera, so overlays at a height line up as it turns.
    lua.set_function("ViewLift", [](float x, float y, float z) {
        const Vector2 p = ViewLift(x, y, z);
        return std::make_tuple(p.x, p.y);
    });
    // ViewProject(x, y, z) -> x, y: the screen pixel of that point, for Screen2D ("ui") drawing.
    lua.set_function("ViewProject", [](float x, float y, float z) {
        const Vector2 p = ViewProject(x, y, z);
        return std::make_tuple(p.x, p.y);
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

    // GetEntityByName: nil when there is no such entity.
    //
    // It used to return 0 for "not found", but ids start at 0, so the first entity a scene loads
    // was indistinguishable from failure -- a scene whose camera was authored first lost all
    // camera control. nil is also the safer sentinel in Lua, where 0 is truthy, so `if e then`
    // guards silently passed on a miss.
    lua.set_function("GetEntityByName", [this](const std::string& name) -> sol::object {
        auto* world = GetActiveWorld();
        if (!world) return sol::nil;
        Entity entity = INVALID_ENTITY;
        if (!world->GetEntityByName(name, &entity)) return sol::nil;
        return sol::make_object(lua, entity);
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

            if (!world->HasComponent<TransformComponent>(e)) continue;

            auto& transform = world->GetComponent<TransformComponent>(e);
            float dx = transform.worldX - x;
            float dy = transform.worldY - y;
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
        auto* scene = s_services->Get<ISceneService>().GetTopScene();
        if (!scene) return false;

        auto* collisionSystem = scene->GetSystem<Elysium::Systems::CollisionSystem>();
        if (!collisionSystem) return false;

        return collisionSystem->AreColliding(a, b);
    });

    lua.set_function("IssueMoveCommand", [](Entity entity, float x, float y) {
        auto* scene = s_services->Get<ISceneService>().GetTopScene();
        if (!scene) return;

        auto* movementSystem = scene->GetSystem<Elysium::Systems::MovementSystem>();
        if (!movementSystem) return;

        movementSystem->IssueMoveCommand(entity, {x, y});
    });

    // Navmesh queries — the scene-level walkability layer (NavMeshSystem).
    lua.set_function("NavIsWalkable", [](float x, float y) -> bool {
        auto* scene = s_services->Get<ISceneService>().GetTopScene();
        if (!scene) return false;
        auto* nav = scene->GetSystem<Elysium::Systems::NavMeshSystem>();
        return nav && nav->IsWalkable({x, y});
    });
    lua.set_function("NavSetDebugDraw", [](bool enabled) {
        auto* scene = s_services->Get<ISceneService>().GetTopScene();
        if (!scene) return;
        if (auto* nav = scene->GetSystem<Elysium::Systems::NavMeshSystem>()) nav->SetParameter("debugDraw", Value{enabled});
    });
    auto topNav = []() -> Elysium::Systems::NavMeshSystem* {
        auto* scene = s_services->Get<ISceneService>().GetTopScene();
        return scene ? scene->GetSystem<Elysium::Systems::NavMeshSystem>() : nullptr;
    };
    // NavFloorHeight(x, y [, z]): the walkable floor's height there (nearest `z`), or nil.
    lua.set_function("NavFloorHeight", [this, topNav](float x, float y, sol::optional<float> z) -> sol::object {
        auto* nav = topNav();
        if (!nav) return sol::nil;
        if (auto h = nav->FloorHeight({x, y}, z.value_or(0.0f))) return sol::make_object(lua, *h);
        return sol::nil;
    });
    // NavCanWalk(x1, y1, x2, y2 [, z]): whether a unit at height z walks straight from 1 to 2.
    lua.set_function("NavCanWalk", [topNav](float x1, float y1, float x2, float y2, sol::optional<float> z) -> bool {
        auto* nav = topNav();
        return nav && nav->HasLineOfSight({x1, y1}, {x2, y2}, z.value_or(0.0f));
    });
    // NavPick(x, y): the walkable floor drawn at picture point (x, y), as {x, y, z}, or nil.
    lua.set_function("NavPick", [this, topNav](float x, float y) -> sol::object {
        auto* nav = topNav();
        if (!nav) return sol::nil;
        const Vector2 lift = ViewLift(0.0f, 0.0f, 1.0f) - ViewLift(0.0f, 0.0f, 0.0f);
        auto p = nav->PickFloor({x, y}, lift);
        if (!p) return sol::nil;
        sol::table t = lua.create_table();
        t["x"] = p->x; t["y"] = p->y; t["z"] = p->z;
        return t;
    });
    // NavFindPath(x1, y1, x2, y2 [, z1]): waypoints {x, y, z}; empty if there's no way.
    lua.set_function("NavFindPath", [this](float x1, float y1, float x2, float y2, sol::optional<float> z1) -> sol::table {
        sol::table result = lua.create_table();
        auto* scene = s_services->Get<ISceneService>().GetTopScene();
        if (!scene) return result;
        auto* nav = scene->GetSystem<Elysium::Systems::NavMeshSystem>();
        if (!nav) return result;
        int i = 1;
        for (const auto& p : nav->FindPath({x1, y1}, {x2, y2}, z1.value_or(0.0f))) {
            sol::table pt = lua.create_table();
            pt["x"] = p.x; pt["y"] = p.y; pt["z"] = p.z;
            result[i++] = pt;
        }
        return result;
    });

    // NavReach(x, y, z, budget [, blockers]): everywhere a unit at (x, y, z) walks within
    // `budget` ground units, kept out of `blockers` ({ {x, y, r}, ... }). Returns a reach map:
    //   reach:Cost(x, y, z)   -> the walking cost to that ground point, or nil
    //   reach:PathTo(x, y, z) -> waypoints {x, y, z} to it (start excluded), empty if unreached
    //   reach:Runs()          -> { {x0, x1, y, z, cost}, ... } row runs of reached ground, to draw
    using Reach = Elysium::Systems::NavMeshSystem::Reach;
    auto reachType = lua.new_usertype<Reach>("NavReachMap", sol::no_constructor);
    reachType["budget"] = sol::readonly(&Reach::budget);
    reachType["Cost"] = [topNav](const Reach& r, float x, float y, sol::optional<float> z) -> sol::optional<float> {
        auto* nav = topNav();
        if (!nav) return sol::nullopt;
        if (auto c = nav->ReachCost(r, {x, y}, z.value_or(0.0f))) return *c;
        return sol::nullopt;
    };
    reachType["PathTo"] = [this, topNav](const Reach& r, float x, float y, sol::optional<float> z) -> sol::table {
        sol::table result = lua.create_table();
        auto* nav = topNav();
        if (!nav) return result;
        int i = 1;
        for (const auto& p : nav->ReachPath(r, {x, y}, z.value_or(0.0f))) {
            sol::table pt = lua.create_table();
            pt["x"] = p.x; pt["y"] = p.y; pt["z"] = p.z;
            result[i++] = pt;
        }
        return result;
    };
    reachType["Runs"] = [this, topNav](const Reach& r) -> sol::table {
        sol::table result = lua.create_table();
        auto* nav = topNav();
        if (!nav) return result;
        int i = 1;
        for (const auto& run : nav->ReachRuns(r)) {
            sol::table t = lua.create_table();
            t["x0"] = run.x0; t["x1"] = run.x1; t["y"] = run.y; t["z"] = run.z; t["cost"] = run.cost;
            result[i++] = t;
        }
        return result;
    };
    lua.set_function("NavReach", [topNav](float x, float y, float z, float budget, sol::optional<sol::table> blockers)
                                     -> std::shared_ptr<Reach> {
        auto* nav = topNav();
        if (!nav) return nullptr;
        std::vector<Vector3> circles;
        if (blockers) {
            for (auto& [_, v] : *blockers) {
                if (!v.is<sol::table>()) continue;
                sol::table b = v.as<sol::table>();
                circles.push_back({b.get_or("x", 0.0f), b.get_or("y", 0.0f), b.get_or("r", 0.0f)});
            }
        }
        auto reach = std::make_shared<Reach>(nav->ComputeReach({x, y}, z, budget, circles));
        if (reach->start < 0) return nullptr;
        return reach;
    });
    // NavCanSee(x1, y1, z1, x2, y2, z2): whether the sight line between the two points (z is
    // the eye height, not the floor's) is clear of terrain and static colliders.
    lua.set_function("NavCanSee", [topNav](float x1, float y1, float z1, float x2, float y2, float z2) -> bool {
        auto* nav = topNav();
        return !nav || nav->CanSee({x1, y1, z1}, {x2, y2, z2});
    });
    // NavGroundDistance(x1, y1, x2, y2): distance on the ground (the metric NavReach budgets use).
    lua.set_function("NavGroundDistance", [topNav](float x1, float y1, float x2, float y2) -> float {
        auto* nav = topNav();
        if (!nav) {
            const float dx = x2 - x1, dy = (y2 - y1) * 2.0f;
            return std::sqrt(dx * dx + dy * dy);
        }
        return nav->GroundDistance({x1, y1}, {x2, y2});
    });

    lua.set_function("GetCollisions", [this](Entity entity) -> sol::table {
        sol::table result = lua.create_table();

        auto* scene = s_services->Get<ISceneService>().GetTopScene();
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

    // Deferred draw commands — queued here, fulfilled by RenderSystem at the correct time
    // Color is passed as a Lua table: { r=255, g=255, b=255, a=255 }
    auto tableToColor = [](sol::table t) -> Color {
        return Color{
            (unsigned char)t.get_or("r", 255),
            (unsigned char)t.get_or("g", 255),
            (unsigned char)t.get_or("b", 255),
            (unsigned char)t.get_or("a", 255)
        };
    };

    lua.set_function("DrawCircle", [tableToColor](float x, float y, float radius, sol::table color, const std::string& layer) {
        if (auto* rs = GetCurrentRenderSystem()) {
            rs->IssueDrawCommand(Elysium::Systems::DrawCircleCmd{layer, x, y, radius, tableToColor(color)});
        }
    });

    lua.set_function("DrawEllipse", [tableToColor](float x, float y, float radiusH, float radiusV, sol::table color, const std::string& layer) {
        if (auto* rs = GetCurrentRenderSystem()) {
            rs->IssueDrawCommand(Elysium::Systems::DrawEllipseCmd{layer, x, y, radiusH, radiusV, tableToColor(color)});
        }
    });

    lua.set_function("DrawLine", [tableToColor](float x1, float y1, float x2, float y2, sol::table color, const std::string& layer) {
        if (auto* rs = GetCurrentRenderSystem()) {
            rs->IssueDrawCommand(Elysium::Systems::DrawLineCmd{layer, x1, y1, x2, y2, tableToColor(color)});
        }
    });

    // DrawPolygon(points, color, layer)
    // points is an array of {x, y} tables — pass outline verts in order, no need for a center point.
    lua.set_function("DrawPolygon", [tableToColor](sol::table points, sol::table color, const std::string& layer) {
        auto* rs = GetCurrentRenderSystem();
        if (!rs) return;
        Elysium::Systems::DrawPolygonCmd cmd;
        cmd.layer = layer;
        cmd.color = tableToColor(color);
        points.for_each([&](sol::object /*key*/, sol::object val) {
            if (val.is<sol::table>()) {
                sol::table pt = val.as<sol::table>();
                cmd.points.push_back({pt.get_or("x", 0.0f), pt.get_or("y", 0.0f)});
            }
        });
        rs->IssueDrawCommand(std::move(cmd));
    });

    lua.set_function("DrawText", [tableToColor](const std::string& text, float x, float y, int fontSize, sol::table color, const std::string& layer) {
        if (auto* rs = GetCurrentRenderSystem()) {
            rs->IssueDrawCommand(Elysium::Systems::DrawTextCmd{layer, text, x, y, fontSize, tableToColor(color)});
        }
    });

    lua.set_function("FillRect", [tableToColor](float x, float y, float width, float height, sol::table color, const std::string& layer) {
        if (auto* rs = GetCurrentRenderSystem()) {
            rs->IssueDrawCommand(Elysium::Systems::DrawRectCmd{layer, x, y, width, height, tableToColor(color)});
        }
    });
}

void ScriptService::BindInputConstants() {
    // Keyboard Keys — pulled from Elysium::Key so this table can never drift from
    // what Input::IsKeyDown/IsKeyPressed above actually accept.
    lua["KEY_SPACE"] = static_cast<int>(Key::Space);
    lua["KEY_ENTER"] = static_cast<int>(Key::Enter);
    lua["KEY_TAB"] = static_cast<int>(Key::Tab);
    lua["KEY_ESCAPE"] = static_cast<int>(Key::Escape);
    lua["KEY_BACKSPACE"] = static_cast<int>(Key::Backspace);
    lua["KEY_LEFT"] = static_cast<int>(Key::Left);
    lua["KEY_RIGHT"] = static_cast<int>(Key::Right);
    lua["KEY_UP"] = static_cast<int>(Key::Up);
    lua["KEY_DOWN"] = static_cast<int>(Key::Down);

    for (int i = 0; i < 26; ++i) {
        char name[8];
        snprintf(name, sizeof(name), "KEY_%c", 'A' + i);
        lua[name] = static_cast<int>(Key::A) + i;
    }

    for (int i = 0; i < 10; ++i) {
        char name[8];
        snprintf(name, sizeof(name), "KEY_%d", i);
        lua[name] = static_cast<int>(Key::Zero) + i;
    }

    // Mouse Buttons
    lua["MOUSE_LEFT"] = static_cast<int>(MouseButton::Left);
    lua["MOUSE_RIGHT"] = static_cast<int>(MouseButton::Right);
    lua["MOUSE_MIDDLE"] = static_cast<int>(MouseButton::Middle);
}

sol::table ScriptService::GetOrLoadScript(Path path) {
    ProfileN("ScriptService GetOrLoadScript");
    ProfileText(path.c_str());
    auto it = scriptRegistry.find(path);
    if (it != scriptRegistry.end()) {
        return it->second;
    }

    auto& assetService = s_services->Get<IAssetService>();
    auto* script = assetService.Get<Script>(path);
    if (!script) {
        LOG_ERRORF("ScriptService", "Failed to load script: %s. Error: Asset not found", path.c_str());
        return sol::nil;
    }

    if (script->source.empty()) {
        LOG_ERRORF("ScriptService", "Failed to load script: %s. Error: Script is empty", path.c_str());
        return sol::nil;
    }

    auto result = lua.load(script->source);
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
    ProfileN("ScriptService InitializeEntity");
    ProfileText(scriptName.c_str());
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
    ProfileN("ScriptService UpdateEntity");
    ProfileText(scriptName.c_str());
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
    ProfileN("ScriptService OnEntityEvent");
    ProfileText(scriptPath.c_str());
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

sol::table ScriptService::GetSceneInstance(Path scriptPath, bool create) {
    auto it = sceneScriptInstances.find(scriptPath);
    if (it != sceneScriptInstances.end()) return it->second;
    if (!create) return sol::nil;

    sol::table proto = GetOrLoadScript(scriptPath);
    if (!proto.valid()) return sol::nil;

    sol::table instance = lua.create_table();
    sol::table mt = lua.create_table();
    mt["__index"] = proto;
    instance[sol::metatable_key] = mt;

    sceneScriptInstances[scriptPath] = instance;
    return instance;
}

bool ScriptService::WarnMissingSceneHook(const Path& scriptPath, const char* hook) {
    // Once per script/hook: these are called every frame, so an unconditional warning would bury
    // the log. Silence here is what hid a scene script that was loading and initializing but
    // never ticking.
    if (warnedMissingHooks_.insert(std::string(scriptPath.c_str()) + ":" + hook).second) {
        LOG_WARNINGF("ScriptService", "Scene script %s has no %s", scriptPath.c_str(), hook);
    }
    return false;
}

bool ScriptService::InitializeScene(Path scriptPath) {
    ProfileN("ScriptService InitializeScene");
    ProfileText(scriptPath.c_str());
    sol::table instance = GetSceneInstance(scriptPath, true);
    if (!instance.valid()) return false;

    sol::function initFunc = instance["Initialize"];
    if (!initFunc.valid()) {
        // A scene script with no Initialize is legal but almost always a mistake (a chunk that
        // returned the wrong table, or a typo'd method name), and it used to succeed silently.
        WarnMissingSceneHook(scriptPath, "Initialize");
        return true;
    }
    auto result = initFunc(instance);
    if (!result.valid()) {
        sol::error err = result;
        LOG_ERRORF("ScriptService", "Error in scene %s:Initialize: %s", scriptPath.c_str(), err.what());
        return false;
    }
    LOG_INFOF("ScriptService", "Scene script %s initialized", scriptPath.c_str());
    return true;
}

bool ScriptService::UpdateScene(Path scriptPath, float deltaTime) {
    ProfileN("ScriptService UpdateScene");
    ProfileText(scriptPath.c_str());
    sol::table instance = GetSceneInstance(scriptPath, false);
    if (!instance.valid()) return WarnMissingSceneHook(scriptPath, "instance (not initialized)");

    sol::function updateFunc = instance["Update"];
    if (!updateFunc.valid()) return WarnMissingSceneHook(scriptPath, "Update");
    {
        auto result = updateFunc(instance, deltaTime);
        if (!result.valid()) {
            sol::error err = result;
            LOG_ERRORF("ScriptService", "Error in scene %s:Update: %s", scriptPath.c_str(), err.what());
            return false;
        }
    }
    return true;
}

bool ScriptService::RenderScene(Path scriptPath) {
    ProfileN("ScriptService RenderScene");
    ProfileText(scriptPath.c_str());
    sol::table instance = GetSceneInstance(scriptPath, false);
    if (!instance.valid()) return WarnMissingSceneHook(scriptPath, "instance (not initialized)");

    sol::function renderFunc = instance["Render"];
    if (!renderFunc.valid()) return WarnMissingSceneHook(scriptPath, "Render");
    {
        auto result = renderFunc(instance);
        if (!result.valid()) {
            sol::error err = result;
            LOG_ERRORF("ScriptService", "Error in scene %s:Render: %s", scriptPath.c_str(), err.what());
            return false;
        }
    }
    return true;
}

void ScriptService::OnSceneEvent(Path scriptPath, Event& event) {
    ProfileN("ScriptService OnSceneEvent");
    ProfileText(scriptPath.c_str());
    sol::table instance = GetSceneInstance(scriptPath, false);
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

    auto result = onEventFunc(instance, eventData);
    if (!result.valid()) {
        sol::error err = result;
        LOG_ERRORF("ScriptService", "Error in scene %s:OnEvent: %s", scriptPath.c_str(), err.what());
        return;
    }
    // If the Lua handler returns true, mark the event handled to stop propagation.
    if (result.return_count() > 0) {
        auto retVal = result.get<sol::object>(0);
        if (retVal.is<bool>() && retVal.as<bool>()) {
            event.handled = true;
        }
    }
}

void ScriptService::ReloadScript(Path scriptPath) {
    scriptRegistry.erase(scriptPath);
    LOG_INFOF("ScriptService", "Unloaded script: %s", scriptPath.c_str());
}

namespace {
    // `entity`'s instance table of `scriptPath`, if it has one.
    std::optional<sol::table> FindInstance(
        std::unordered_map<Entity, std::unordered_map<Path, sol::table>>& instances, Entity entity, const Path& scriptPath) {
        auto entityIt = instances.find(entity);
        if (entityIt == instances.end()) return std::nullopt;
        auto scriptIt = entityIt->second.find(scriptPath);
        if (scriptIt == entityIt->second.end()) return std::nullopt;
        return scriptIt->second;
    }
}

std::optional<std::vector<ScriptField>> ScriptService::GetScriptFields(Entity entity, Path scriptPath) {
    auto instance = FindInstance(entityScriptInstances, entity, scriptPath);
    if (!instance) return std::nullopt;

    std::vector<ScriptField> fields;
    for (auto& [key, val] : *instance) {
        if (!key.is<std::string>()) continue;
        std::string name = key.as<std::string>();
        if (name.empty() || name[0] == '_') continue;

        ScriptField field{name, std::nullopt, ""};
        if (val.is<sol::function>()) field.kind = "function";
        else if (val.is<sol::table>()) field.kind = "table";
        else if (val.is<float>() || val.is<double>()) field.value = val.as<float>();
        else if (val.is<int>()) field.value = val.as<int>();
        else if (val.is<bool>()) field.value = val.as<bool>();
        else if (val.is<std::string>()) field.value = val.as<std::string>();
        else field.kind = "unknown type";
        fields.push_back(std::move(field));
    }
    return fields;
}

bool ScriptService::SetScriptField(Entity entity, Path scriptPath, const std::string& name, const ScriptValue& value) {
    auto instance = FindInstance(entityScriptInstances, entity, scriptPath);
    if (!instance) return false;
    std::visit([&](const auto& v) { (*instance)[name] = v; }, value);
    return true;
}

} // namespace Elysium::Services
