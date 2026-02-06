#pragma once
#include "Core/Component.h"

namespace Elysium {
    struct SelectionComponent {
        SelectionComponent() = default;

        static constexpr const char* Name() { return "Selection"; }
        static constexpr const char* XmlTag() { return "SelectionComponent"; }

        static void LoadXml(SelectionComponent& c, tinyxml2::XMLElement* el);
        static void Inspect(SelectionComponent& c, Entity e);
        static void BindLua(sol::usertype<SelectionComponent>& ut);
        static void SetFromLua(SelectionComponent& c, sol::object v);
    };
}
