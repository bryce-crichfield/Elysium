#pragma once

#include <functional>
#include <typeindex>
#include <unordered_map>

#include "Core/Entity.h"
#include "Core/Reflection.h"
#include "Core/World.h"
#include "Editor/Inspectors/FieldInspector.h"

namespace Elysium {

class ServiceLocator;

// Where a component's section sits in the Inspector, top to bottom: what the entity is,
// where it is, what it looks like, how it behaves. Ties sort by name.
enum class InspectorOrder : int {
    Identity,   // Name
    Transform,
    Hierarchy,  // Parent
    Geometry,   // shapes, sprites, text, models, bounds
    Layer,
    Material,
    Shader,
    Rendering,  // camera, UI
    Physics,    // collision and motion
    Gameplay,
    Scripting,
    Other,
};

// Draws one component's section of the Inspector; nothing for an entity without it.
using Inspector = std::function<void(World&, Entity, ServiceLocator&)>;

// The component inspectors, by component type. Components know nothing about the editor: each
// one's inspector is registered here, by the editor (RegisterComponentInspectors), and the
// Inspector panel finds it through the type the ComponentRegistry records for the component.
class InspectorRegistry {
public:
    struct Entry {
        Inspector draw;
        InspectorOrder order = InspectorOrder::Other;
    };

    // A hand-written inspector over the live component.
    template <typename T>
    void Register(InspectorOrder order, void (*inspect)(T&, Entity, ServiceLocator&)) {
        entries_[std::type_index(typeid(T))] = Entry{
            [inspect](World& world, Entity e, ServiceLocator& services) {
                if (world.HasComponent<T>(e)) inspect(world.GetComponent<T>(e), e, services);
            },
            order};
    }

    // The generic inspector: one row per typed field (Core/Reflection.h).
    template <Reflected T>
    void Register(InspectorOrder order) {
        entries_[std::type_index(typeid(T))] = Entry{
            [fields = T::Fields()](World& world, Entity e, ServiceLocator&) {
                if (world.HasComponent<T>(e)) InspectFields(&world.GetComponent<T>(e), fields);
            },
            order};
    }

    // Null for a component nothing registered, which the Inspector leaves out.
    const Entry* Find(std::type_index type) const {
        auto it = entries_.find(type);
        return it == entries_.end() ? nullptr : &it->second;
    }

private:
    std::unordered_map<std::type_index, Entry> entries_;
};

// Registers every component's inspector (Editor/Inspectors/ComponentInspectors.cpp).
void RegisterComponentInspectors(InspectorRegistry& registry);

}  // namespace Elysium
