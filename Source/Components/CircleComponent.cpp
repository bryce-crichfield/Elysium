#include "Components/CircleComponent.h"
#include "Core/ComponentRegistry.h"
#include "Core/Xml.h"
#include "imgui.h"

namespace Elysium {
    CircleComponent::CircleComponent(float r, Color background, Color border, const std::string& layer)
        : radius(r), background(background), border(border), layerName(layer) {}

    void CircleComponent::LoadXml(CircleComponent& c, tinyxml2::XMLElement* el) {
        c.radius = el->FloatAttribute("radius", 50.0f);
        std::string fillHex = el->Attribute("fill") ? el->Attribute("fill") : "";
        std::string borderHex = el->Attribute("border") ? el->Attribute("border") : "";
        const char* layerName = el->Attribute("layerName");
        c.layerName = layerName ? layerName : "tile";

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

        static char layerBuffer[256];
        strncpy(layerBuffer, c.layerName.c_str(), sizeof(layerBuffer) - 1);
        layerBuffer[sizeof(layerBuffer) - 1] = '\0';

        Label("Layer Name: ");
        if (ImGui::InputText("##LayerName", layerBuffer, sizeof(layerBuffer))) {
            c.layerName = std::string(layerBuffer);
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
        ut["layerName"] = &CircleComponent::layerName;
    }

    void CircleComponent::SetFromLua(CircleComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.radius = t.get_or("radius", c.radius);
            c.layerName = t.get_or("layerName", c.layerName);
            if (t["background"].valid()) c.background = ObjectToColor(t["background"]);
            if (t["border"].valid()) c.border = ObjectToColor(t["border"]);
        }
    }

    REGISTER_COMPONENT(CircleComponent);
}
