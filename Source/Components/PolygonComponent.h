#pragma once
#include "Core/Component.h"
#include "raylib.h"
#include <vector>

namespace Elysium {
    struct PolygonComponent {
        std::vector<Vector2> points;  // local-space, relative to entity position
        Color fill;
        Color border;

        PolygonComponent(std::vector<Vector2> points = {}, Color fill = {}, Color border = {});

        static constexpr const char* Name() { return "Polygon"; }
        static constexpr const char* XmlTag() { return "PolygonComponent"; }

        static void LoadXml(PolygonComponent& c, tinyxml2::XMLElement* el);
        static void SaveXml(const PolygonComponent& c, XMLBuilder& builder);
        static void Inspect(PolygonComponent& c, Entity e);
        static void BindLua(sol::usertype<PolygonComponent>& ut);
        static void SetFromLua(PolygonComponent& c, sol::object v);
    };
}
