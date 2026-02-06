#include "Components/HealthComponent.h"
#include "Core/ComponentRegistry.h"
#include "imgui.h"

namespace Elysium {
    void HealthComponent::LoadXml(HealthComponent& c, tinyxml2::XMLElement* el) {
        float maxVal = el->FloatAttribute("max", 100.0f);
        c.max = maxVal;
        c.current = maxVal;
    }

    void HealthComponent::Inspect(HealthComponent& c, Entity e) {
        auto Label = [](const char* label) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(label);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1);
        };

        Label("Current: ");
        ImGui::DragFloat("##CurrentHealth", &c.current, 1.0f, 0.0f, c.max);
        Label("Max: ");
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
