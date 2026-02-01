#include "Components/UnitComponent.h"
#include "Core/ComponentRegistry.h"
#include "imgui.h"

namespace Elysium {
    void UnitComponent::LoadXml(UnitComponent& c, tinyxml2::XMLElement* el) {
        // No explicit XML loading in original
    }

    void UnitComponent::Inspect(UnitComponent& c, Entity e) {
        auto Label = [](const char* label) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(label);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1);
        };

        Label("Acted: ");
        ImGui::Checkbox("##HasActedThisTurn", &c.hasActedThisTurn);
        Label("Can Move: ");
        ImGui::Checkbox("##CanMove", &c.canMove);
        Label("Can Attack: ");
        ImGui::Checkbox("##CanAttack", &c.canAttack);
        Label("Can Cast Spells: ");
        ImGui::Checkbox("##CanCastSpells", &c.canCastSpells);
        Label("Can Use Items: ");
        ImGui::Checkbox("##CanUseItems", &c.canUseItems);

        if (ImGui::Button("Start Turn")) {
            c.StartTurn();
        }
        ImGui::SameLine();
        if (ImGui::Button("End Turn")) {
            c.EndTurn();
        }
    }

    void UnitComponent::BindLua(sol::usertype<UnitComponent>& ut) {
        ut["hasActedThisTurn"] = &UnitComponent::hasActedThisTurn;
        ut["canMove"] = &UnitComponent::canMove;
        ut["canAttack"] = &UnitComponent::canAttack;
        ut["canCastSpells"] = &UnitComponent::canCastSpells;
        ut["canUseItems"] = &UnitComponent::canUseItems;
        ut["StartTurn"] = &UnitComponent::StartTurn;
        ut["EndTurn"] = &UnitComponent::EndTurn;
    }

    void UnitComponent::SetFromLua(UnitComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.hasActedThisTurn = t.get_or("hasActedThisTurn", c.hasActedThisTurn);
            c.canMove = t.get_or("canMove", c.canMove);
            c.canAttack = t.get_or("canAttack", c.canAttack);
            c.canCastSpells = t.get_or("canCastSpells", c.canCastSpells);
            c.canUseItems = t.get_or("canUseItems", c.canUseItems);
        }
    }

    REGISTER_COMPONENT(UnitComponent);
}
