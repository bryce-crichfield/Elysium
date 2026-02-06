#pragma once
#include "Core/Component.h"
#include "Sprite.h"
#include <string>

namespace Elysium {
    struct SpriteComponent {
        Sprite sprite;
        std::string markerName;
        int frameIndex = 0;          // Current frame within the marker
        float frameDuration = 0.2f;  // Time per frame in seconds
        float frameElapsed = 0.0f;   // Time elapsed on current frame

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
