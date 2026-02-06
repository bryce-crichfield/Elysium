#pragma once
#include "Core/Component.h"

namespace Elysium {
    struct LocationComponent {
        int x, y;

        LocationComponent(int x = 0, int y = 0);

        static constexpr const char* Name() { return "Location"; }
        static constexpr const char* XmlTag() { return "LocationComponent"; }

        static void LoadXml(LocationComponent& c, tinyxml2::XMLElement* el);
        static void Inspect(LocationComponent& c, Entity e);
        static void BindLua(sol::usertype<LocationComponent>& ut);
        static void SetFromLua(LocationComponent& c, sol::object v);
    };
}
