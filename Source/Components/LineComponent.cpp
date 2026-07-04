#include "Components/LineComponent.h"
#include "Core/ComponentRegistry.h"
#include "Core/Xml.h"
#include "imgui.h"

namespace Elysium {
    LineComponent::LineComponent(float x1, float y1, float x2, float y2, Color color, float thickness)
        : x1(x1), y1(y1), x2(x2), y2(y2), color(color), thickness(thickness) {}

    void LineComponent::SaveXml(const LineComponent& c, XMLBuilder& builder) {
        auto b = builder.AddElement("LineComponent")
            .SetAttribute("x1", c.x1)
            .SetAttribute("y1", c.y1)
            .SetAttribute("x2", c.x2)
            .SetAttribute("y2", c.y2)
            .SetAttribute("thickness", c.thickness);
        std::string colorHex = ColorToHex(c.color);
        if (!colorHex.empty()) b.SetAttribute("color", colorHex.c_str());
    }

    void LineComponent::LoadXml(LineComponent& c, tinyxml2::XMLElement* el) {
        c.x1 = el->FloatAttribute("x1", 0.0f);
        c.y1 = el->FloatAttribute("y1", 0.0f);
        c.x2 = el->FloatAttribute("x2", 50.0f);
        c.y2 = el->FloatAttribute("y2", 0.0f);
        c.thickness = el->FloatAttribute("thickness", 1.0f);
        std::string colorHex = el->Attribute("color") ? el->Attribute("color") : "";
        c.color = ParseHexColor(colorHex, WHITE);
    }

    static Color ObjectToColor(const sol::object& obj) {
        if (obj.is<Color>()) return obj.as<Color>();
        if (obj.is<std::string>()) return ParseHexColor(obj.as<std::string>(), WHITE);
        return WHITE;
    }

    void LineComponent::Inspect(LineComponent& c, Entity e) {
        auto Label = [](const char* label) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(label);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1);
        };

        Label("X1: ");
        ImGui::DragFloat("##X1", &c.x1, 1.0f);
        Label("Y1: ");
        ImGui::DragFloat("##Y1", &c.y1, 1.0f);
        Label("X2: ");
        ImGui::DragFloat("##X2", &c.x2, 1.0f);
        Label("Y2: ");
        ImGui::DragFloat("##Y2", &c.y2, 1.0f);

        float color[4] = {c.color.r / 255.0f, c.color.g / 255.0f, c.color.b / 255.0f, c.color.a / 255.0f};
        Label("Color: ");
        if (ImGui::ColorEdit4("##Color", color)) {
            c.color = {(unsigned char)(color[0] * 255), (unsigned char)(color[1] * 255),
                       (unsigned char)(color[2] * 255), (unsigned char)(color[3] * 255)};
        }

        Label("Thickness: ");
        ImGui::DragFloat("##Thickness", &c.thickness, 0.1f, 0.1f, 50.0f);
    }

    void LineComponent::BindLua(sol::usertype<LineComponent>& ut) {
        ut["x1"] = &LineComponent::x1;
        ut["y1"] = &LineComponent::y1;
        ut["x2"] = &LineComponent::x2;
        ut["y2"] = &LineComponent::y2;
        ut["thickness"] = &LineComponent::thickness;
        ut["color"] = sol::property(
            [](LineComponent& c) { return c.color; },
            [](LineComponent& c, sol::object v) { c.color = ObjectToColor(v); });
    }

    void LineComponent::SetFromLua(LineComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.x1 = t.get_or("x1", c.x1);
            c.y1 = t.get_or("y1", c.y1);
            c.x2 = t.get_or("x2", c.x2);
            c.y2 = t.get_or("y2", c.y2);
            c.thickness = t.get_or("thickness", c.thickness);
            if (t["color"].valid()) c.color = ObjectToColor(t["color"]);
        }
    }

    REGISTER_COMPONENT(LineComponent);
}
