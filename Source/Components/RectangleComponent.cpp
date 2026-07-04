#include "Components/RectangleComponent.h"
#include "Core/ComponentRegistry.h"
#include "Core/Xml.h"
#include "imgui.h"

namespace Elysium {
    RectangleComponent::RectangleComponent(float width, float height, Color background, Color border, const std::string& textureName,
                                            float strokeWidth, float cornerRadius)
        : width(width), height(height), background(background), border(border), textureName(textureName),
          strokeWidth(strokeWidth), cornerRadius(cornerRadius) {}

    void RectangleComponent::SaveXml(const RectangleComponent& c, XMLBuilder& builder) {
        auto b = builder.AddElement("RectangleComponent")
            .SetAttribute("width", c.width)
            .SetAttribute("height", c.height)
            .SetAttribute("strokeWidth", c.strokeWidth)
            .SetAttribute("cornerRadius", c.cornerRadius);
        std::string bgHex = ColorToHex(c.background);
        std::string borderHex = ColorToHex(c.border);
        if (!bgHex.empty()) b.SetAttribute("background", bgHex.c_str());
        if (!borderHex.empty()) b.SetAttribute("border", borderHex.c_str());
        if (!c.textureName.empty()) b.SetAttribute("texture", c.textureName.c_str());
    }

    void RectangleComponent::LoadXml(RectangleComponent& c, tinyxml2::XMLElement* el) {
        c.width = el->FloatAttribute("width", 100.0f);
        c.height = el->FloatAttribute("height", 100.0f);
        c.strokeWidth = el->FloatAttribute("strokeWidth", 1.0f);
        c.cornerRadius = el->FloatAttribute("cornerRadius", 0.0f);
        std::string backgroundHex = el->Attribute("background") ? el->Attribute("background") : "";
        std::string borderHex = el->Attribute("border") ? el->Attribute("border") : "";
        std::string textureName = el->Attribute("texture") ? el->Attribute("texture") : "";
        c.textureName = textureName;
        c.background = ParseHexColor(backgroundHex, BLANK);
        c.border = ParseHexColor(borderHex, BLANK);
    }

    static Color ObjectToColor(const sol::object& obj) {
        if (obj.is<Color>()) return obj.as<Color>();
        if (obj.is<std::string>()) return ParseHexColor(obj.as<std::string>(), BLANK);
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

        Label("Stroke Width: ");
        ImGui::DragFloat("##StrokeWidth", &c.strokeWidth, 0.1f, 0.0f, 50.0f);
        Label("Corner Radius: ");
        ImGui::DragFloat("##CornerRadius", &c.cornerRadius, 0.01f, 0.0f, 1.0f);

        Label("Texture: ");
        char buffer[256];
        std::strncpy(buffer, c.textureName.c_str(), sizeof(buffer));
        if (ImGui::InputText("##Texture", buffer, sizeof(buffer))) {
            c.textureName = buffer;
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
        ut["textureName"] = &RectangleComponent::textureName;
        ut["strokeWidth"] = &RectangleComponent::strokeWidth;
        ut["cornerRadius"] = &RectangleComponent::cornerRadius;
    }

    void RectangleComponent::SetFromLua(RectangleComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.width = t.get_or("width", c.width);
            c.height = t.get_or("height", c.height);
            c.strokeWidth = t.get_or("strokeWidth", c.strokeWidth);
            c.cornerRadius = t.get_or("cornerRadius", c.cornerRadius);
            if (t["background"].valid()) c.background = ObjectToColor(t["background"]);
            if (t["border"].valid()) c.border = ObjectToColor(t["border"]);
            if (t["textureName"].valid()) c.textureName = t["textureName"];
        }
    }

    REGISTER_COMPONENT(RectangleComponent);
}
