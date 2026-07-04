#include "Components/EllipseComponent.h"
#include "Core/ComponentRegistry.h"
#include "Core/Xml.h"
#include "imgui.h"

namespace Elysium {
    EllipseComponent::EllipseComponent(float radiusH, float radiusV, Color background, Color border)
        : radiusH(radiusH), radiusV(radiusV), background(background), border(border) {}

    void EllipseComponent::SaveXml(const EllipseComponent& c, XMLBuilder& builder) {
        auto b = builder.AddElement("EllipseComponent")
            .SetAttribute("radiusH", c.radiusH)
            .SetAttribute("radiusV", c.radiusV);
        std::string fillHex = ColorToHex(c.background);
        std::string borderHex = ColorToHex(c.border);
        if (!fillHex.empty()) b.SetAttribute("fill", fillHex.c_str());
        if (!borderHex.empty()) b.SetAttribute("border", borderHex.c_str());
    }

    void EllipseComponent::LoadXml(EllipseComponent& c, tinyxml2::XMLElement* el) {
        c.radiusH = el->FloatAttribute("radiusH", 50.0f);
        c.radiusV = el->FloatAttribute("radiusV", 50.0f);
        std::string fillHex = el->Attribute("fill") ? el->Attribute("fill") : "";
        std::string borderHex = el->Attribute("border") ? el->Attribute("border") : "";

        c.background = ParseHexColor(fillHex, BLANK);
        c.border = ParseHexColor(borderHex, BLANK);
    }

    static Color ObjectToColor(const sol::object& obj) {
        if (obj.is<Color>()) return obj.as<Color>();
        if (obj.is<std::string>()) return ParseHexColor(obj.as<std::string>(), BLANK);
        return WHITE;
    }

    void EllipseComponent::Inspect(EllipseComponent& c, Entity e) {
        auto Label = [](const char* label) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(label);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1);
        };

        Label("Radius H: ");
        ImGui::DragFloat("##RadiusH", &c.radiusH, 1.0f, 1.0f, 1000.0f);
        Label("Radius V: ");
        ImGui::DragFloat("##RadiusV", &c.radiusV, 1.0f, 1.0f, 1000.0f);

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

    void EllipseComponent::BindLua(sol::usertype<EllipseComponent>& ut) {
        ut["radiusH"] = &EllipseComponent::radiusH;
        ut["radiusV"] = &EllipseComponent::radiusV;
        ut["background"] = sol::property(
            [](EllipseComponent& c) { return c.background; },
            [](EllipseComponent& c, sol::object v) { c.background = ObjectToColor(v); });
        ut["border"] = sol::property(
            [](EllipseComponent& c) { return c.border; },
            [](EllipseComponent& c, sol::object v) { c.border = ObjectToColor(v); });
    }

    void EllipseComponent::SetFromLua(EllipseComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.radiusH = t.get_or("radiusH", c.radiusH);
            c.radiusV = t.get_or("radiusV", c.radiusV);
            if (t["background"].valid()) c.background = ObjectToColor(t["background"]);
            if (t["border"].valid()) c.border = ObjectToColor(t["border"]);
        }
    }

    REGISTER_COMPONENT(EllipseComponent);
}
