#pragma once
#include "Core/Component.h"
#include "Core/Math/Polygon.h"
#include "Core/Math/MathTypes.h"
#include <string>
#include <vector>

namespace Elysium {
    enum class NavAreaType { Walkable, Blocked, Cost };

    const char* ToString(NavAreaType type);
    NavAreaType ParseNavAreaType(const std::string& text);

    struct NavAreaComponent {
        std::string points;
        std::string type = "Walkable";
        float cost = 1.0f;

        NavAreaType Type() const { return ParseNavAreaType(type); }
        Polygon LocalPolygon() const;

        static constexpr const char* Name() { return "Nav Area"; }
        static constexpr const char* XmlTag() { return "NavAreaComponent"; }

        static void LoadXml(NavAreaComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services);
        static void SaveXml(const NavAreaComponent& c, XMLBuilder& builder);
        static FieldList Fields();
        static void BindLua(sol::usertype<NavAreaComponent>& ut);
        static void SetFromLua(NavAreaComponent& c, sol::object v);
    };
}
