#pragma once
#include "Core/Component.h"
#include "raylib.h"
#include <string>

namespace Elysium {
    struct FollowComponent {
        float speed = 1.0f;
        std::string targetEntityName;

        static constexpr const char* Name() { return "Follow"; }
        static constexpr const char* XmlTag() { return "FollowComponent"; }

        static void LoadXml(FollowComponent& c, tinyxml2::XMLElement* el);
        static void Inspect(FollowComponent& c, Entity e);
        static void BindLua(sol::usertype<FollowComponent>& ut);
        static void SetFromLua(FollowComponent& c, sol::object v);
    };
}
