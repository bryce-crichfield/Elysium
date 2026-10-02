#include "Core/Components/TeamComponent.h"
#include "Core/ComponentRegistry.h"
#include "tinyxml2.h"

namespace Elysium {

    void TeamComponent::LoadXml(TeamComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services) {
        c.team = el->IntAttribute("team", 0);
    }

    void TeamComponent::SaveXml(const TeamComponent& c, XMLBuilder& builder) {
        builder.AddElement("TeamComponent")
            .SetAttribute("team", c.team);
    }

    FieldList TeamComponent::Fields() {
        return {
            Field("Team", &TeamComponent::team, "team"),
        };
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
