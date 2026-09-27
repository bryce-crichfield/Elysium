#pragma once
#include "Core/Component.h"

namespace Elysium {
    struct TeamComponent {
        int team = 0;  // 0 = player, 1 = enemy

        TeamComponent(int team = 0) : team(team) {}

        static constexpr const char* Name() { return "Team"; }
        static constexpr InspectorOrder Order = InspectorOrder::Gameplay;
        static constexpr const char* XmlTag() { return "TeamComponent"; }

        static void LoadXml(TeamComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services);
        static void SaveXml(const TeamComponent& c, XMLBuilder& builder);
        static FieldList Fields();
        static void BindLua(sol::usertype<TeamComponent>& ut);
        static void SetFromLua(TeamComponent& c, sol::object v);
    };
}
