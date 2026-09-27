#include "Components/TextComponent.h"
#include "Core/ComponentRegistry.h"
#include "Editor/Widgets.h"
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

    void TextComponent::LoadXml(TextComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services) {
        c.content = el->Attribute("text") ? el->Attribute("text") : "";
        c.fontSize = el->IntAttribute("fontSize", 12);
        std::string colorHex = el->Attribute("color") ? el->Attribute("color") : "";
        c.color = ParseHexColor(colorHex, Colors::White);
    }

    static Color ObjectToColor(const sol::object& obj) {
        if (obj.is<Color>()) return obj.as<Color>();
        return Colors::White;
    }

    FieldList TextComponent::Fields() {
        return {
            Field("Content", &TextComponent::content, "text").Multiline(),
            Field("Font Size", &TextComponent::fontSize, "fontSize").Range(1.0f, 200.0f),
            Field("Color", &TextComponent::color, "color"),
        };
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
