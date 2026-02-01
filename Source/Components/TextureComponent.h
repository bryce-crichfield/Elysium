#pragma once
#include "Core/Component.h"
#include "raylib.h"
#include <string>

namespace Elysium {
    struct TextureComponent {
        std::string textureName;  // Asset name of the texture
        std::string layerName = "default";
        Rectangle clip = {0, 0, 0, 0};  // Source clip rect (0,0,0,0 = use full texture)
        Color tint = WHITE;             // Tint/modulation color

        TextureComponent() = default;
        TextureComponent(const std::string& texture, const std::string& layer = "default");

        static constexpr const char* Name() { return "Texture"; }
        static constexpr const char* XmlTag() { return "TextureComponent"; }

        static void LoadXml(TextureComponent& c, tinyxml2::XMLElement* el);
        static void Inspect(TextureComponent& c, Entity e);
        static void BindLua(sol::usertype<TextureComponent>& ut);
        static void SetFromLua(TextureComponent& c, sol::object v);
    };
}
