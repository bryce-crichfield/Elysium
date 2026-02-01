#pragma once
#include "Core/Component.h"

namespace Elysium {
    struct CharacterComponent {
        int id;

        static constexpr const char* Name() { return "Character"; }
        static constexpr const char* XmlTag() { return "CharacterComponent"; }

        static void LoadXml(CharacterComponent& c, tinyxml2::XMLElement* el);
        static void Inspect(CharacterComponent& c, Entity e);
        static void BindLua(sol::usertype<CharacterComponent>& ut);
        static void SetFromLua(CharacterComponent& c, sol::object v);
    };
}
