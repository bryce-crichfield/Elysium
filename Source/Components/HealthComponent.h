#pragma once
#include "Core/Component.h"

namespace Elysium {
    struct HealthComponent {
        float current;
        float max;
    
        HealthComponent(float maxHealth = 100.0f) : current(maxHealth), max(maxHealth) {}

        static constexpr const char* Name() { return "Health"; }
        static constexpr InspectorOrder Order = InspectorOrder::Gameplay;
        static constexpr const char* XmlTag() { return "HealthComponent"; }

        static void LoadXml(HealthComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services);
        static void Inspect(HealthComponent& c, Entity e, ServiceLocator& services);
        static void BindLua(sol::usertype<HealthComponent>& ut);
        static void SetFromLua(HealthComponent& c, sol::object v);
    };
}
