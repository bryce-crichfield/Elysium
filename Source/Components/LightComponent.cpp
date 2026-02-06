#include "Components/LightComponent.h"
#include "Core/ComponentRegistry.h"
#include "imgui.h"

namespace Elysium {
    void LightComponent::LoadXml(LightComponent& c, tinyxml2::XMLElement* el) {
        c.radius = el->FloatAttribute("radius", 50.0f);
        c.intensity = el->FloatAttribute("intensity", 0.5f);
        int r = el->IntAttribute("r", 255);
        int g = el->IntAttribute("g", 255);
        int b = el->IntAttribute("b", 255);
        int a = el->IntAttribute("a", 100);
        c.color = {(unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a};
    }

    static Color ObjectToColor(const sol::object& obj) {
        if (obj.is<Color>()) return obj.as<Color>();
        return WHITE;
    }

    void LightComponent::Inspect(LightComponent& c, Entity e) {
        auto Label = [](const char* label) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(label);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1);
        };

        float color[4] = {c.color.r / 255.0f, c.color.g / 255.0f, c.color.b / 255.0f, c.color.a / 255.0f};
        Label("Color: ");
        if (ImGui::ColorEdit4("##Color", color)) {
            c.color = {(unsigned char)(color[0] * 255), (unsigned char)(color[1] * 255),
                       (unsigned char)(color[2] * 255), (unsigned char)(color[3] * 255)};
        }

        Label("Radius: ");
        ImGui::DragFloat("##Radius", &c.radius, 1.0f, 1.0f, 1000.0f);

        Label("Intensity: ");
        ImGui::SliderFloat("##Intensity", &c.intensity, 0.0f, 1.0f);
    }

    void LightComponent::BindLua(sol::usertype<LightComponent>& ut) {
        ut["radius"] = &LightComponent::radius;
        ut["intensity"] = &LightComponent::intensity;
        ut["color"] = sol::property(
            [](LightComponent& c) { return c.color; },
            [](LightComponent& c, sol::object v) { c.color = ObjectToColor(v); });
    }

    void LightComponent::SetFromLua(LightComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.radius = t.get_or("radius", c.radius);
            c.intensity = t.get_or("intensity", c.intensity);
            if (t["color"].valid()) c.color = ObjectToColor(t["color"]);
        }
    }

    REGISTER_COMPONENT(LightComponent);
}
