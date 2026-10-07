#pragma once
#include "Core/Component.h"
#include <string>

namespace Elysium {
    struct NameComponent {
        std::string name;
        NameComponent(const std::string& name = "");

        static constexpr const char* Name() { return "Name"; }
        static constexpr bool PlacementOwned = true;
        static constexpr const char* XmlTag() { return "NameComponent"; }

        static void LoadXml(NameComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services);
        static void SaveXml(const NameComponent& c, XMLBuilder& builder);
        static FieldList Fields();
        static void BindLua(sol::usertype<NameComponent>& ut);
        static void SetFromLua(NameComponent& c, sol::object v);
    };
}
