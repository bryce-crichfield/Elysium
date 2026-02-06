#pragma once
#include "Core/Component.h"
#include "Sprite.h"
#include <string>

namespace Elysium {
    struct SpriteComponent {
        std::string spriteName;
        std::string sheetName;
        std::string sequenceName;
        int sequenceIndex = 0;          
        float frameDuration = 0.2f;  
        float frameElapsed = 0.0f;  

        SpriteComponent() = default;
        SpriteComponent(const Sprite& sprite, const std::string& marker);

        static constexpr const char* Name() { return "Sprite"; }
        static constexpr const char* XmlTag() { return "SpriteComponent"; }

        static void LoadXml(SpriteComponent& c, tinyxml2::XMLElement* el);
        static void Inspect(SpriteComponent& c, Entity e);
        static void BindLua(sol::usertype<SpriteComponent>& ut);
        static void SetFromLua(SpriteComponent& c, sol::object v);
    };
}
