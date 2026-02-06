#include "Components/AttackComponent.h"
#include "Core/ComponentRegistry.h"
#include "imgui.h"

namespace Elysium {
    void AttackComponent::LoadXml(AttackComponent& c, tinyxml2::XMLElement* el) {
        c.range = el->FloatAttribute("range", 100.0f);
        c.damage = el->FloatAttribute("damage", 10.0f);
        c.cooldown = el->FloatAttribute("cooldown", 1.0f);
    }

    void AttackComponent::Inspect(AttackComponent& c, Entity e) {
        auto Label = [](const char* label) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(label);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1);
        };
        
        Label("Range: ");
        ImGui::DragFloat("##Range", &c.range, 1.0f, 0.0f);
        Label("Damage: ");
        ImGui::DragFloat("##Damage", &c.damage, 1.0f, 0.0f);
        Label("Cooldown: ");
        ImGui::DragFloat("##Cooldown", &c.cooldown, 0.1f, 0.0f);
        Label("Timer: ");
        ImGui::DragFloat("##Timer", &c.timer, 0.1f, 0.0f);
        Label("Target ID: ");
        int tId = (int)c.targetId;
        ImGui::DragInt("##TargetId", &tId, 1.0f, 0);
        c.targetId = (Entity)tId;
        Label("Is Attacking: ");
        ImGui::Checkbox("##IsAttacking", &c.isAttacking);
    }

    void AttackComponent::BindLua(sol::usertype<AttackComponent>& ut) {
        ut["damage"] = &AttackComponent::damage;
        ut["range"] = &AttackComponent::range;
        ut["cooldown"] = &AttackComponent::cooldown;
        ut["timer"] = &AttackComponent::timer;
        ut["isAttacking"] = &AttackComponent::isAttacking;
        ut["targetId"] = &AttackComponent::targetId;
    }

    void AttackComponent::SetFromLua(AttackComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.damage = t.get_or("damage", c.damage);
            c.range = t.get_or("range", c.range);
            c.cooldown = t.get_or("cooldown", c.cooldown);
            c.timer = t.get_or("timer", c.timer);
            c.isAttacking = t.get_or("isAttacking", c.isAttacking);
            c.targetId = t.get_or("targetId", c.targetId);
        }
    }

    REGISTER_COMPONENT(AttackComponent);
}
