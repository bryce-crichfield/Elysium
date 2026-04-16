#pragma once

#include "Core/Component.h"
#include <string>

namespace Elysium {

struct CharacterComponent {
    std::string characterPath;   // e.g. "Characters/Archer.xml"

    static constexpr const char* Name()   { return "Character"; }
    static constexpr const char* XmlTag() { return "CharacterComponent"; }

    static void LoadXml(CharacterComponent& c, tinyxml2::XMLElement* el);
    static void SaveXml(const CharacterComponent& c, XMLBuilder& builder);
    static void Inspect(CharacterComponent& c, Entity e);
    static void BindLua(sol::usertype<CharacterComponent>& ut);
    static void SetFromLua(CharacterComponent& c, sol::object v);
};

}  // namespace Elysium
