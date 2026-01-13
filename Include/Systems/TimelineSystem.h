#pragma once

#include "System.h"
#include "Timeline.h"
#include "Component.h"
#include <functional>
#include <vector>
#include <string>
#include <typeindex>

namespace Elysium::Systems {

// Property accessor for applying track values to components
struct PropertyAccessor {
    std::string componentName;
    std::string propertyName;
    std::type_index valueType;
    std::function<void(Entity, World*, const void*)> applyFunc;
    std::function<bool(Entity, World*)> hasComponentFunc;

    PropertyAccessor(const std::string& comp, const std::string& prop, std::type_index type)
        : componentName(comp), propertyName(prop), valueType(type) {}
};

class TimelineSystem : public System {
private:
    std::vector<PropertyAccessor> accessors_;

    // Register a property accessor
    template<typename ComponentType, typename ValueType>
    void RegisterProperty(
        const std::string& componentName,
        const std::string& propertyName,
        std::function<void(ComponentType&, const ValueType&)> setter
    ) {
        PropertyAccessor accessor(componentName, propertyName, std::type_index(typeid(ValueType)));

        accessor.applyFunc = [setter](Entity entity, World* world, const void* valuePtr) {
            auto& component = world->GetComponent<ComponentType>(entity);
            const ValueType& value = *static_cast<const ValueType*>(valuePtr);
            setter(component, value);
        };

        accessor.hasComponentFunc = [](Entity entity, World* world) {
            return world->HasComponent<ComponentType>(entity);
        };

        accessors_.push_back(accessor);
    }

    // Find and bind an accessor to a track
    void BindTrackAccessor(TrackBase* track);

public:
    TimelineSystem(Context context);

    void Update(float deltaTime) override;

    // Get registered property names for a component (for GUI dropdown)
    std::vector<std::string> GetPropertiesForComponent(const std::string& componentName) const;
};

} // namespace Elysium::Systems
