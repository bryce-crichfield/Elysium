#include "Core/Components/TextComponent.h"
#include "Core/ComponentRegistry.h"
#include "Core/Xml.h"

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
        if (!c.font.empty()) builder.SetAttribute("font", c.font.c_str());
    }

    void TextComponent::LoadXml(TextComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services) {
        c.content = el->Attribute("text") ? el->Attribute("text") : "";
        c.fontSize = el->IntAttribute("fontSize", 12);
        std::string colorHex = el->Attribute("color") ? el->Attribute("color") : "";
        c.color = ParseHexColor(colorHex, Colors::White);
        if (colorHex.empty() && el->Attribute("r")) {
            c.color = Color{(unsigned char)el->IntAttribute("r", 255), (unsigned char)el->IntAttribute("g", 255),
                            (unsigned char)el->IntAttribute("b", 255), (unsigned char)el->IntAttribute("a", 255)};
        }
        c.font = el->Attribute("font") ? el->Attribute("font") : "";
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
            Field("Font", &TextComponent::font, "font").Asset(AssetKind::Font),
        };
    }

    void TextComponent::BindLua(sol::usertype<TextComponent>& ut) {
        ut["content"] = &TextComponent::content;
        ut["fontSize"] = &TextComponent::fontSize;
        ut["font"] = &TextComponent::font;
        ut["color"] = sol::property(
            [](TextComponent& t) { return t.color; },
            [](TextComponent& t, sol::object v) { t.color = ObjectToColor(v); });
    }

    void TextComponent::SetFromLua(TextComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.content = t.get_or("content", c.content);
            c.fontSize = t.get_or("fontSize", c.fontSize);
            c.font = t.get_or("font", c.font);
            if (t["color"].valid()) c.color = ObjectToColor(t["color"]);
        }
    }

    REGISTER_COMPONENT(TextComponent);
}
