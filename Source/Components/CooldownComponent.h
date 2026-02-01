#pragma once
#include "Core/Component.h"

namespace Elysium {
    struct CooldownComponent {
        float cooldownTime = 0.0f;  // Time in seconds
        float elapsedTime = 0.0f;   // Time elapsed since last action
        bool isOnCooldown = false;  // Is the action currently on cooldown

        void SetCooldown(float duration);

        CooldownComponent();
        CooldownComponent(float cooldown);

        static constexpr const char* Name() { return "Cooldown"; }
        static constexpr const char* XmlTag() { return "CooldownComponent"; }

        static void LoadXml(CooldownComponent& c, tinyxml2::XMLElement* el);
        static void Inspect(CooldownComponent& c, Entity e);
        static void BindLua(sol::usertype<CooldownComponent>& ut);
        static void SetFromLua(CooldownComponent& c, sol::object v);
    };
}
