#pragma once
#include "Core/Component.h"
#include "Core/Graphics.h"
#include <string>

namespace Elysium {
    struct TextComponent {
        std::string content;
        int fontSize;
        Color color;

        TextComponent(const std::string& text = "", int size = 20, Color c = {});

        static constexpr const char* Name() { return "Text"; }
        static constexpr const char* XmlTag() { return "TextComponent"; }

        static void LoadXml(TextComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services);
        static void SaveXml(const TextComponent& c, XMLBuilder& builder);
        static FieldList Fields();
        static void BindLua(sol::usertype<TextComponent>& ut);
        static void SetFromLua(TextComponent& c, sol::object v);
    };
}
