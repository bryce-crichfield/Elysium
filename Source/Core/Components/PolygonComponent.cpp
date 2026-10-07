#include "Core/Components/PolygonComponent.h"
#include "Core/ComponentRegistry.h"
#include "Core/Xml.h"
#include <sstream>

namespace Elysium {

    static std::string FormatPoints(const std::vector<Vector2>& points) {
        std::ostringstream ss;
        for (size_t i = 0; i < points.size(); i++) {
            if (i > 0) ss << ' ';
            ss << points[i].x << ',' << points[i].y;
        }
        return ss.str();
    }

    static std::vector<Vector2> ParsePoints(const std::string& s) {
        std::vector<Vector2> points;
        std::istringstream tokens(s);
        std::string token;
        while (tokens >> token) {
            size_t comma = token.find(',');
            if (comma == std::string::npos) continue;
            try {
                float x = std::stof(token.substr(0, comma));
                float y = std::stof(token.substr(comma + 1));
                points.push_back({x, y});
            } catch (...) {
                continue;
            }
        }
        return points;
    }

    PolygonComponent::PolygonComponent(std::vector<Vector2> points) : points(std::move(points)) {}

    void PolygonComponent::SaveXml(const PolygonComponent& c, XMLBuilder& builder) {
        builder.AddElement("PolygonComponent")
            .SetAttribute("points", FormatPoints(c.points).c_str());
    }

    void PolygonComponent::LoadXml(PolygonComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services) {
        std::string pointsStr = el->Attribute("points") ? el->Attribute("points") : "";
        c.points = ParsePoints(pointsStr);
    }

    void PolygonComponent::BindLua(sol::usertype<PolygonComponent>& ut) {
        ut["pointCount"] = sol::readonly_property([](PolygonComponent& c) { return (int)c.points.size(); });
    }

    void PolygonComponent::SetFromLua(PolygonComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            if (t["points"].valid() && t["points"].is<sol::table>()) {
                sol::table pts = t["points"];
                c.points.clear();
                pts.for_each([&](sol::object /*key*/, sol::object val) {
                    if (val.is<sol::table>()) {
                        sol::table pt = val.as<sol::table>();
                        c.points.push_back({pt.get_or("x", 0.0f), pt.get_or("y", 0.0f)});
                    }
                });
            }
        }
    }

    REGISTER_COMPONENT(PolygonComponent);
}
