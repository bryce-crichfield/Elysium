#include "Components/RectangleComponent.h"
#include "Core/ComponentRegistry.h"
#include "Core/Xml.h"
#include "imgui.h"

namespace Elysium {
    RectangleComponent::RectangleComponent(float width, float height, Color background, Color border)
        : width(width), height(height), background(background), border(border) {}

    void RectangleComponent::LoadXml(RectangleComponent& c, tinyxml2::XMLElement* el) {
        c.width = el->FloatAttribute("width", 100.0f);
        c.height = el->FloatAttribute("height", 100.0f);
        std::string backgroundHex = el->Attribute("background") ? el->Attribute("background") : "";
        std::string borderHex = el->Attribute("border") ? el->Attribute("border") : "";

        c.background = ParseHexColor(backgroundHex, BLANK);
        c.border = ParseHexColor(borderHex, BLANK);
    }

    static Color ObjectToColor(const sol::object& obj) {
        if (obj.is<Color>()) return obj.as<Color>();
        // Helper fallback if needed or strict type
        return WHITE;
    }

    void RectangleComponent::Inspect(RectangleComponent& c, Entity e) {
        auto Label = [](const char* label) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(label);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1);
        };

        Label("Width: ");
        ImGui::DragFloat("##Width", &c.width, 1.0f, 1.0f, 1000.0f);
        Label("Height: ");
        ImGui::DragFloat("##Height", &c.height, 1.0f, 1.0f, 1000.0f);

        float bg[4] = {c.background.r / 255.0f, c.background.g / 255.0f, c.background.b / 255.0f,
                       c.background.a / 255.0f};
        Label("Background: ");
        if (ImGui::ColorEdit4("##Background", bg)) {
            c.background = {(unsigned char)(bg[0] * 255), (unsigned char)(bg[1] * 255), (unsigned char)(bg[2] * 255),
                            (unsigned char)(bg[3] * 255)};
        }

        float border[4] = {c.border.r / 255.0f, c.border.g / 255.0f, c.border.b / 255.0f, c.border.a / 255.0f};
        Label("Border: ");
        if (ImGui::ColorEdit4("##Border", border)) {
            c.border = {(unsigned char)(border[0] * 255), (unsigned char)(border[1] * 255),
                        (unsigned char)(border[2] * 255), (unsigned char)(border[3] * 255)};
        }
    }

    void RectangleComponent::BindLua(sol::usertype<RectangleComponent>& ut) {
        ut["width"] = &RectangleComponent::width;
        ut["height"] = &RectangleComponent::height;
        ut["background"] = sol::property(
            [](RectangleComponent& r) { return r.background; },
            [](RectangleComponent& r, sol::object v) { r.background = ObjectToColor(v); });
        ut["border"] = sol::property(
            [](RectangleComponent& r) { return r.border; },
            [](RectangleComponent& r, sol::object v) { r.border = ObjectToColor(v); });
    }

    void RectangleComponent::SetFromLua(RectangleComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.width = t.get_or("width", c.width);
            c.height = t.get_or("height", c.height);
            if (t["background"].valid()) c.background = ObjectToColor(t["background"]);
            if (t["border"].valid()) c.border = ObjectToColor(t["border"]);
        }
    }

    REGISTER_COMPONENT(RectangleComponent);
}
