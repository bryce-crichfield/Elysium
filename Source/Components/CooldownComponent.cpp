#include "Components/CooldownComponent.h"
#include "Core/ComponentRegistry.h"
#include "imgui.h"

namespace Elysium {
    CooldownComponent::CooldownComponent() = default;
    CooldownComponent::CooldownComponent(float cooldown) : cooldownTime(cooldown) {}

    void CooldownComponent::SetCooldown(float duration) {
        cooldownTime = duration;
        elapsedTime = 0.0f;
        isOnCooldown = true;
    }

    void CooldownComponent::LoadXml(CooldownComponent& c, tinyxml2::XMLElement* el) {
        // No loading
    }

    void CooldownComponent::Inspect(CooldownComponent& c, Entity e) {
        auto Label = [](const char* label) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(label);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1);
        };

        Label("Cooldown Time: ");
        ImGui::DragFloat("##CooldownTime", &c.cooldownTime, 0.1f, 0.0f, 60.0f);
        Label("Elapsed Time: ");
        ImGui::DragFloat("##ElapsedTime", &c.elapsedTime, 0.01f, 0.0f);
        Label("On Cooldown: ");
        ImGui::Checkbox("##IsOnCooldown", &c.isOnCooldown);
    }

    void CooldownComponent::BindLua(sol::usertype<CooldownComponent>& ut) {
        ut["cooldownTime"] = &CooldownComponent::cooldownTime;
        ut["elapsedTime"] = &CooldownComponent::elapsedTime;
        ut["isOnCooldown"] = &CooldownComponent::isOnCooldown;
        ut["SetCooldown"] = &CooldownComponent::SetCooldown;
    }

    void CooldownComponent::SetFromLua(CooldownComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.cooldownTime = t.get_or("cooldownTime", c.cooldownTime);
            c.elapsedTime = t.get_or("elapsedTime", c.elapsedTime);
            c.isOnCooldown = t.get_or("isOnCooldown", c.isOnCooldown);
        }
    }

    REGISTER_COMPONENT(CooldownComponent);
}
