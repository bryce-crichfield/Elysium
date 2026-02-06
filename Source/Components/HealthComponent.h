#pragma once
#include "Core/Component.h"

namespace Elysium {
    struct HealthComponent {
        float current;
        float max;
    
        HealthComponent(float maxHealth = 100.0f) : current(maxHealth), max(maxHealth) {}

        static constexpr const char* Name() { return "Health"; }
        static constexpr const char* XmlTag() { return "HealthComponent"; }

        static void LoadXml(HealthComponent& c, tinyxml2::XMLElement* el);
        static void Inspect(HealthComponent& c, Entity e);
        static void BindLua(sol::usertype<HealthComponent>& ut);
        static void SetFromLua(HealthComponent& c, sol::object v);
    };
}
