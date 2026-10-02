#include "Core/Components/HealthComponent.h"
#include "Core/ComponentRegistry.h"

namespace Elysium {
    void HealthComponent::LoadXml(HealthComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services) {
        float maxVal = el->FloatAttribute("max", 100.0f);
        c.max = maxVal;
        c.current = maxVal;
    }

    FieldList HealthComponent::Fields() {
        return {
            Field("Max", &HealthComponent::max, "max").Range(1.0f, 100000.0f),
            Field("Current", &HealthComponent::current),
        };
    }

    void HealthComponent::BindLua(sol::usertype<HealthComponent>& ut) {
        ut["current"] = &HealthComponent::current;
        ut["max"] = &HealthComponent::max;
    }

    void HealthComponent::SetFromLua(HealthComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.current = t.get_or("current", c.current);
            c.max = t.get_or("max", c.max);
        }
    }

    REGISTER_COMPONENT(HealthComponent);
}
