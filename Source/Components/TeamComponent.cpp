#include "Components/TeamComponent.h"
#include "Core/ComponentRegistry.h"
#include "imgui.h"

namespace Elysium {
    TeamComponent::TeamComponent() : teamId(0) {}
    TeamComponent::TeamComponent(int teamId) : teamId(teamId) {}

    void TeamComponent::LoadXml(TeamComponent& c, tinyxml2::XMLElement* el) {
        c.teamId = el->IntAttribute("teamId", 0);
    }

    void TeamComponent::Inspect(TeamComponent& c, Entity e) {
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Team ID: ");
        ImGui::SameLine(140.0f);
        ImGui::SetNextItemWidth(-1);
        ImGui::DragInt("##TeamID", &c.teamId, 1.0f, 0);
    }

    void TeamComponent::BindLua(sol::usertype<TeamComponent>& ut) {
        ut["teamId"] = &TeamComponent::teamId;
    }

    void TeamComponent::SetFromLua(TeamComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.teamId = t.get_or("teamId", c.teamId);
        }
    }

    REGISTER_COMPONENT(TeamComponent);
}
