#pragma once
#include "Core/Component.h"
#include <string>

namespace Elysium {
    struct FactionComponent {
        std::string name;
        
        FactionComponent(const std::string& name = "Player") : name(name) {}

        static constexpr const char* Name() { return "Faction"; }
        static constexpr const char* XmlTag() { return "FactionComponent"; }

        static void LoadXml(FactionComponent& c, tinyxml2::XMLElement* el);
        static void Inspect(FactionComponent& c, Entity e);
        static void BindLua(sol::usertype<FactionComponent>& ut);
        static void SetFromLua(FactionComponent& c, sol::object v);
    };
}
