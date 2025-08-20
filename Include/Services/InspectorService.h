#pragma once

#include "Entity.h"
#include "Component.h"
#include "imgui.h"
#include "rlImGui.h"
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <set>
#include <unordered_map>
#include <string>
#include <variant>

namespace Elysium {

class Application;
class Scene;
class World;

enum class InspectorLogicalOperator { AND, OR };

struct InspectorFilter {
    std::string componentName;
    bool negate = false;
    InspectorLogicalOperator logicalOperator = InspectorLogicalOperator::OR;
    std::function<bool(Entity, World*)> predicate;

    InspectorFilter() = default;
    InspectorFilter(const std::string& name, bool neg, InspectorLogicalOperator op, std::function<bool(Entity, World*)> pred)
        : componentName(name), negate(neg), logicalOperator(op), predicate(pred) {}

    bool Evaluate(Entity entity, World* world) const {
        return negate ? !predicate(entity, world) : predicate(entity, world);
    }
};


class InspectorService
{
private:
    Entity selectedEntity = INVALID_ENTITY;
    std::string searchFilter = "";
    bool showInspector = true;
    World* currentWorld = nullptr;
    World* world = nullptr;
    std::string componentToDelete = "";

    struct ComponentPlaceholder {
        std::function<void(Entity, World*)> drawFunc;
        std::function<bool(Entity, World*)> hasComponentFunc;
        std::function<void(Entity, World*)> addComponentFunc;
        std::function<void(Entity, World*)> removeComponentFunc;
        std::function<void(Entity, World*)> resetComponentFunc;
        std::string name;
    };

    std::vector<ComponentPlaceholder> componentPlaceholders;
    std::vector<InspectorFilter> filters;
    bool filtersCollapsed = true;
    float leftPanelWidth = 240.0f; // 2/5 of 600px default width
    bool isDraggingSplitter = false;

    void RegisterComponentTypes();
    void DrawEntityList();
    void DrawInspectorPanel();
    void DrawEntityToolbar();
    void DrawInspectorToolbar();
    void DrawComponentPanel(const ComponentPlaceholder& placeholder);

    void DeselectEntity();
    void RemoveEntity();
    void CreateEntity();

    template<typename T>
    void RegisterComponent(const std::string& name);

    template<typename T>
    void DrawComponent(Entity entity, World* world);

public:
    InspectorService() = default;
    ~InspectorService() = default;

    void Initialize();
    void Shutdown();
    void Update(float deltaTime);
    void Draw();

    void SetCurrentWorld(World* world) { currentWorld = world; this->world = world; }
    void SetSelectedEntity(Entity entity) { selectedEntity = entity; }
    Entity GetSelectedEntity() const { return selectedEntity; }
    void ToggleVisibility() { showInspector = !showInspector; }
    bool IsVisible() const { return showInspector; }
};

template<typename T>
void InspectorService::RegisterComponent(const std::string& name)
{
    ComponentPlaceholder placeholder;
    placeholder.name = name;

    placeholder.drawFunc = [this](Entity entity, World* world) { this->DrawComponent<T>(entity, world); };
    placeholder.hasComponentFunc = [](Entity entity, World* world) { return world->HasComponent<T>(entity); };
    placeholder.addComponentFunc = [](Entity entity, World* world) { world->AddComponent<T>(entity, T{}); };
    placeholder.removeComponentFunc = [](Entity entity, World* world) { world->RemoveComponent<T>(entity); };
    placeholder.resetComponentFunc = [](Entity entity, World* world) {
        world->RemoveComponent<T>(entity);
        world->AddComponent<T>(entity, T{});
    };

    componentPlaceholders.push_back(placeholder);
}

} // namespace Elysium
