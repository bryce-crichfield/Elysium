#include "Components/PolygonComponent.h"
#include "Core/ComponentRegistry.h"
#include "Core/Xml.h"
#include "imgui.h"
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

    PolygonComponent::PolygonComponent(std::vector<Vector2> points, Color fill, Color border)
        : points(std::move(points)), fill(fill), border(border) {}

    void PolygonComponent::SaveXml(const PolygonComponent& c, XMLBuilder& builder) {
        auto b = builder.AddElement("PolygonComponent")
            .SetAttribute("points", FormatPoints(c.points).c_str());
        std::string fillHex = ColorToHex(c.fill);
        std::string borderHex = ColorToHex(c.border);
        if (!fillHex.empty()) b.SetAttribute("fill", fillHex.c_str());
        if (!borderHex.empty()) b.SetAttribute("border", borderHex.c_str());
    }

    void PolygonComponent::LoadXml(PolygonComponent& c, tinyxml2::XMLElement* el) {
        std::string pointsStr = el->Attribute("points") ? el->Attribute("points") : "";
        c.points = ParsePoints(pointsStr);
        std::string fillHex = el->Attribute("fill") ? el->Attribute("fill") : "";
        std::string borderHex = el->Attribute("border") ? el->Attribute("border") : "";
        c.fill = ParseHexColor(fillHex, BLANK);
        c.border = ParseHexColor(borderHex, BLANK);
    }

    static Color ObjectToColor(const sol::object& obj) {
        if (obj.is<Color>()) return obj.as<Color>();
        if (obj.is<std::string>()) return ParseHexColor(obj.as<std::string>(), BLANK);
        return WHITE;
    }

    void PolygonComponent::Inspect(PolygonComponent& c, Entity e) {
        auto Label = [](const char* label) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(label);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1);
        };

        ImGui::Text("Points: %d", (int)c.points.size());

        float fill[4] = {c.fill.r / 255.0f, c.fill.g / 255.0f, c.fill.b / 255.0f, c.fill.a / 255.0f};
        Label("Fill: ");
        if (ImGui::ColorEdit4("##Fill", fill)) {
            c.fill = {(unsigned char)(fill[0] * 255), (unsigned char)(fill[1] * 255),
                     (unsigned char)(fill[2] * 255), (unsigned char)(fill[3] * 255)};
        }

        float border[4] = {c.border.r / 255.0f, c.border.g / 255.0f, c.border.b / 255.0f, c.border.a / 255.0f};
        Label("Border: ");
        if (ImGui::ColorEdit4("##Border", border)) {
            c.border = {(unsigned char)(border[0] * 255), (unsigned char)(border[1] * 255),
                        (unsigned char)(border[2] * 255), (unsigned char)(border[3] * 255)};
        }
    }

    void PolygonComponent::BindLua(sol::usertype<PolygonComponent>& ut) {
        ut["fill"] = sol::property(
            [](PolygonComponent& c) { return c.fill; },
            [](PolygonComponent& c, sol::object v) { c.fill = ObjectToColor(v); });
        ut["border"] = sol::property(
            [](PolygonComponent& c) { return c.border; },
            [](PolygonComponent& c, sol::object v) { c.border = ObjectToColor(v); });
        ut["pointCount"] = sol::readonly_property([](PolygonComponent& c) { return (int)c.points.size(); });
    }

    void PolygonComponent::SetFromLua(PolygonComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            if (t["fill"].valid()) c.fill = ObjectToColor(t["fill"]);
            if (t["border"].valid()) c.border = ObjectToColor(t["border"]);
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
