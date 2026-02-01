#pragma once
#include "Core/Component.h"
#include "raylib.h"
#include <string>

namespace Elysium {
    struct RectangleComponent {
        float width, height;
        Color background;
        Color border;
        std::string layerName = "default";

        RectangleComponent(float width = 1, float height = 1, Color background = {}, Color border = {}, const std::string& layer = "default");

        static constexpr const char* Name() { return "Rectangle"; }
        static constexpr const char* XmlTag() { return "RectangleComponent"; }

        static void LoadXml(RectangleComponent& c, tinyxml2::XMLElement* el);
        static void Inspect(RectangleComponent& c, Entity e);
        static void BindLua(sol::usertype<RectangleComponent>& ut);
        static void SetFromLua(RectangleComponent& c, sol::object v);
    };
}
