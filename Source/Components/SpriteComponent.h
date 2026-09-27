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
        static constexpr InspectorOrder Order = InspectorOrder::Geometry;
        static constexpr const char* XmlTag() { return "SpriteComponent"; }

        static void LoadXml(SpriteComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services);
        static void SaveXml(const SpriteComponent& c, XMLBuilder& builder);
        static void Inspect(SpriteComponent& c, Entity e, ServiceLocator& services);
        static FieldList Fields();
        static void BindLua(sol::usertype<SpriteComponent>& ut);
        static void SetFromLua(SpriteComponent& c, sol::object v);
    };
}
