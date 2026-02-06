#pragma once
#include "Core/Component.h"

namespace Elysium {
    struct PositionComponent {
        float x, y;
        PositionComponent(float x = 0.0f, float y = 0.0f);

        static constexpr const char* Name() { return "Position"; }
        static constexpr const char* XmlTag() { return "PositionComponent"; }

        static void LoadXml(PositionComponent& c, tinyxml2::XMLElement* el);
        static void Inspect(PositionComponent& c, Entity e);
        static void BindLua(sol::usertype<PositionComponent>& ut);
        static void SetFromLua(PositionComponent& c, sol::object v);
    };
}
