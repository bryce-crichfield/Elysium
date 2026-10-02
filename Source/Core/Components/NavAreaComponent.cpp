#include "Core/Components/NavAreaComponent.h"
#include "Core/ComponentRegistry.h"
#include "Core/Geometry/Polygon.h"
#include "Core/Xml.h"
#include <algorithm>

namespace Elysium {

    const char* ToString(NavAreaType type) {
        switch (type) {
            case NavAreaType::Blocked:  return "Blocked";
            case NavAreaType::Cost:     return "Cost";
            case NavAreaType::Walkable: return "Walkable";
        }
        return "Walkable";
    }

    NavAreaType ParseNavAreaType(const std::string& text) {
        if (text == "Blocked") return NavAreaType::Blocked;
        if (text == "Cost")    return NavAreaType::Cost;
        return NavAreaType::Walkable;
    }

    Polygon NavAreaComponent::LocalPolygon() const {
        Polygon polygon = Polygon::Parse(points);
        return polygon.IsValid() ? polygon : Polygon{};
    }

    void NavAreaComponent::LoadXml(NavAreaComponent& c, tinyxml2::XMLElement* el, ServiceLocator&) {
        c.points = ReadPolygonAttribute(el, "points");
        if (const char* type = el->Attribute("type")) c.type = type;
        c.cost = std::max(1.0f, el->FloatAttribute("cost", 1.0f));
    }

    void NavAreaComponent::SaveXml(const NavAreaComponent& c, XMLBuilder& builder) {
        builder.AddElement("NavAreaComponent")
            .SetAttribute("points", c.points.c_str())
            .SetAttribute("type", c.type.c_str())
            .SetAttribute("cost", c.cost);
    }

    FieldList NavAreaComponent::Fields() {
        return {
            Field("Type", &NavAreaComponent::type, "type"),
            Field("Cost", &NavAreaComponent::cost, "cost").Speed(0.1f).Range(1.0f, 50.0f),
            Field("Polygon", &NavAreaComponent::points, "points"),
        };
    }

    void NavAreaComponent::BindLua(sol::usertype<NavAreaComponent>& ut) {
        ut["points"] = &NavAreaComponent::points;
        ut["type"]   = &NavAreaComponent::type;
        ut["cost"]   = &NavAreaComponent::cost;
    }

    void NavAreaComponent::SetFromLua(NavAreaComponent& c, sol::object v) {
        if (!v.is<sol::table>()) return;
        sol::table t = v.as<sol::table>();
        if (t["points"].valid()) c.points = t["points"].get<std::string>();
        if (t["type"].valid())   c.type   = t["type"].get<std::string>();
        if (t["cost"].valid())   c.cost   = std::max(1.0f, t["cost"].get<float>());
    }

    REGISTER_COMPONENT(NavAreaComponent);
}
