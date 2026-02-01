#pragma once
#include "Core/Component.h"
#include <string>

namespace Elysium {
    struct NameComponent {
        std::string name;
        NameComponent(const std::string& name = "");

        static constexpr const char* Name() { return "Name"; }
        static constexpr const char* XmlTag() { return "NameComponent"; }

        static void LoadXml(NameComponent& c, tinyxml2::XMLElement* el);
        static void Inspect(NameComponent& c, Entity e);
        static void BindLua(sol::usertype<NameComponent>& ut);
        static void SetFromLua(NameComponent& c, sol::object v);
    };
}
