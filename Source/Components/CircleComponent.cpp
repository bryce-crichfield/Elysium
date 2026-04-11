#include "Components/CircleComponent.h"
#include "Core/ComponentRegistry.h"
#include "Core/Xml.h"
#include "imgui.h"

namespace Elysium {
    CircleComponent::CircleComponent(float r, Color background, Color border)
        : radius(r), background(background), border(border) {}

    void CircleComponent::SaveXml(const CircleComponent& c, XMLBuilder& builder) {
        auto b = builder.AddElement("CircleComponent")
            .SetAttribute("radius", c.radius);
        std::string fillHex = ColorToHex(c.background);
        std::string borderHex = ColorToHex(c.border);
        if (!fillHex.empty()) b.SetAttribute("fill", fillHex.c_str());
        if (!borderHex.empty()) b.SetAttribute("border", borderHex.c_str());
    }

    void CircleComponent::LoadXml(CircleComponent& c, tinyxml2::XMLElement* el) {
        c.radius = el->FloatAttribute("radius", 50.0f);
        std::string fillHex = el->Attribute("fill") ? el->Attribute("fill") : "";
        std::string borderHex = el->Attribute("border") ? el->Attribute("border") : "";

        c.background = ParseHexColor(fillHex, BLANK);
        c.border = ParseHexColor(borderHex, BLANK);
    }
    
    static Color ObjectToColor(const sol::object& obj) {
        if (obj.is<Color>()) return obj.as<Color>();
        return WHITE;
    }

    void CircleComponent::Inspect(CircleComponent& c, Entity e) {
        auto Label = [](const char* label) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(label);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1);
        };

        Label("Radius: ");
        ImGui::DragFloat("##Radius", &c.radius, 1.0f, 1.0f, 1000.0f);

        float bg[4] = {c.background.r / 255.0f, c.background.g / 255.0f, c.background.b / 255.0f,
                       c.background.a / 255.0f};
        Label("Background: ");
        if (ImGui::ColorEdit4("##Background", bg)) {
            c.background = {(unsigned char)(bg[0] * 255), (unsigned char)(bg[1] * 255), (unsigned char)(bg[2] * 255),
                            (unsigned char)(bg[3] * 255)};
        }

        float border[4] = {c.border.r / 255.0f, c.border.g / 255.0f, c.border.b / 255.0f,
                           c.border.a / 255.0f};
        Label("Border: ");
        if (ImGui::ColorEdit4("##Border", border)) {
            c.border = {(unsigned char)(border[0] * 255), (unsigned char)(border[1] * 255),
                        (unsigned char)(border[2] * 255), (unsigned char)(border[3] * 255)};
        }
    }

    void CircleComponent::BindLua(sol::usertype<CircleComponent>& ut) {
        ut["radius"] = &CircleComponent::radius;
        ut["background"] = sol::property(
            [](CircleComponent& c) { return c.background; },
            [](CircleComponent& c, sol::object v) { c.background = ObjectToColor(v); });
        ut["border"] = sol::property(
            [](CircleComponent& c) { return c.border; },
            [](CircleComponent& c, sol::object v) { c.border = ObjectToColor(v); });
    }

    void CircleComponent::SetFromLua(CircleComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.radius = t.get_or("radius", c.radius);
            if (t["background"].valid()) c.background = ObjectToColor(t["background"]);
            if (t["border"].valid()) c.border = ObjectToColor(t["border"]);
        }
    }

    REGISTER_COMPONENT(CircleComponent);
}
