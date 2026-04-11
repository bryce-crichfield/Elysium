#pragma once
#include "Core/Component.h"
#include "raylib.h"
#include <string>

namespace Elysium {
    struct TextComponent {
        std::string content;
        int fontSize;
        Color color;

        TextComponent(const std::string& text = "", int size = 20, Color c = {});

        static constexpr const char* Name() { return "Text"; }
        static constexpr const char* XmlTag() { return "TextComponent"; }

        static void LoadXml(TextComponent& c, tinyxml2::XMLElement* el);
        static void SaveXml(const TextComponent& c, XMLBuilder& builder);
        static void Inspect(TextComponent& c, Entity e);
        static void BindLua(sol::usertype<TextComponent>& ut);
        static void SetFromLua(TextComponent& c, sol::object v);
    };
}
