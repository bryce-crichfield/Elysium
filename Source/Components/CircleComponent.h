#pragma once
#include "Core/Component.h"
#include "raylib.h"
#include <string>

namespace Elysium {
    struct CircleComponent {
        float radius;
        Color background;
        Color border;

        CircleComponent(float r = 10.0f, Color background = {}, Color border = {});

        static constexpr const char* Name() { return "Circle"; }
        static constexpr const char* XmlTag() { return "CircleComponent"; }

        static void LoadXml(CircleComponent& c, tinyxml2::XMLElement* el);
        static void SaveXml(const CircleComponent& c, XMLBuilder& builder);
        static void Inspect(CircleComponent& c, Entity e);
        static void BindLua(sol::usertype<CircleComponent>& ut);
        static void SetFromLua(CircleComponent& c, sol::object v);
    };
}
