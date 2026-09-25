#include "Components/HealthComponent.h"
#include "Core/ComponentRegistry.h"
#include "Editor/Widgets.h"
#include "imgui.h"

namespace Elysium {
    void HealthComponent::LoadXml(HealthComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services) {
        float maxVal = el->FloatAttribute("max", 100.0f);
        c.max = maxVal;
        c.current = maxVal;
    }

    void HealthComponent::Inspect(HealthComponent& c, Entity e, ServiceLocator& services) {
        PropertyLabel("Current");
        ImGui::DragFloat("##CurrentHealth", &c.current, 1.0f, 0.0f, c.max);
        PropertyLabel("Max");
        ImGui::DragFloat("##MaxHealth", &c.max, 1.0f, 1.0f);
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
