#pragma once
#include "Core/Component.h"

namespace Elysium {
    struct DirectionComponent {
        DirectionComponent() = default;

        static constexpr const char* Name() { return "Direction"; }
        static constexpr const char* XmlTag() { return "DirectionComponent"; }

        static void LoadXml(DirectionComponent& c, tinyxml2::XMLElement* el);
        static void Inspect(DirectionComponent& c, Entity e);
        static void BindLua(sol::usertype<DirectionComponent>& ut);
        static void SetFromLua(DirectionComponent& c, sol::object v);
    };
}
