#include "Components/TeamComponent.h"
#include "Core/ComponentRegistry.h"
#include "imgui.h"
#include "tinyxml2.h"

namespace Elysium {

    void TeamComponent::LoadXml(TeamComponent& c, tinyxml2::XMLElement* el) {
        c.team = el->IntAttribute("team", 0);
    }

    void TeamComponent::SaveXml(const TeamComponent& c, XMLBuilder& builder) {
        builder.AddElement("TeamComponent")
            .SetAttribute("team", c.team);
    }

    void TeamComponent::Inspect(TeamComponent& c, Entity e) {
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Team:");
        ImGui::SameLine(140.0f);
        ImGui::SetNextItemWidth(-1);
        ImGui::InputInt("##team", &c.team);
    }

    void TeamComponent::BindLua(sol::usertype<TeamComponent>& ut) {
        ut["team"] = &TeamComponent::team;
    }

    void TeamComponent::SetFromLua(TeamComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.team = t.get_or("team", c.team);
        }
    }

    REGISTER_COMPONENT(TeamComponent);
}
