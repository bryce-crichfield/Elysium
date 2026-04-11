#include "Components/TextComponent.h"
#include "Core/ComponentRegistry.h"
#include "Core/Xml.h"
#include "imgui.h"

namespace Elysium {
    TextComponent::TextComponent(const std::string& text, int size, Color c)
        : content(text), fontSize(size), color(c) {}

    void TextComponent::SaveXml(const TextComponent& c, XMLBuilder& builder) {
        builder.AddElement("TextComponent")
            .SetAttribute("text", c.content.c_str())
            .SetAttribute("fontSize", c.fontSize)
            .SetAttribute("r", c.color.r)
            .SetAttribute("g", c.color.g)
            .SetAttribute("b", c.color.b)
            .SetAttribute("a", c.color.a);
    }

    void TextComponent::LoadXml(TextComponent& c, tinyxml2::XMLElement* el) {
        c.content = el->Attribute("text") ? el->Attribute("text") : "";
        c.fontSize = el->IntAttribute("fontSize", 12);
        std::string colorHex = el->Attribute("color") ? el->Attribute("color") : "";
        c.color = ParseHexColor(colorHex, WHITE);
    }

    static Color ObjectToColor(const sol::object& obj) {
        if (obj.is<Color>()) return obj.as<Color>();
        return WHITE;
    }

    void TextComponent::Inspect(TextComponent& c, Entity e) {
        auto Label = [](const char* label) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(label);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1);
        };

        static char contentBuffer[1024];
        strncpy(contentBuffer, c.content.c_str(), sizeof(contentBuffer) - 1);
        contentBuffer[sizeof(contentBuffer) - 1] = '\0';

        Label("Content: ");
        if (ImGui::InputTextMultiline("##Content", contentBuffer, sizeof(contentBuffer))) {
            c.content = std::string(contentBuffer);
        }

        Label("Font Size: ");
        ImGui::DragInt("##FontSize", &c.fontSize, 1.0f, 1, 200);

        float color[4] = {c.color.r / 255.0f, c.color.g / 255.0f, c.color.b / 255.0f, c.color.a / 255.0f};
        Label("Color: ");
        if (ImGui::ColorEdit4("##Color", color)) {
            c.color = {(unsigned char)(color[0] * 255), (unsigned char)(color[1] * 255), (unsigned char)(color[2] * 255),
                          (unsigned char)(color[3] * 255)};
        }
    }

    void TextComponent::BindLua(sol::usertype<TextComponent>& ut) {
        ut["content"] = &TextComponent::content;
        ut["fontSize"] = &TextComponent::fontSize;
        ut["color"] = sol::property(
            [](TextComponent& t) { return t.color; },
            [](TextComponent& t, sol::object v) { t.color = ObjectToColor(v); });
    }

    void TextComponent::SetFromLua(TextComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.content = t.get_or("content", c.content);
            c.fontSize = t.get_or("fontSize", c.fontSize);
            if (t["color"].valid()) c.color = ObjectToColor(t["color"]);
        }
    }

    REGISTER_COMPONENT(TextComponent);
}
