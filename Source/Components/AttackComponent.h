#pragma once
#include "Core/Component.h"
#include "Core/Entity.h" // For Entity alias

namespace Elysium {
    struct AttackComponent {
        float range;
        float damage;
        float cooldown; // seconds
        float timer;    // current countdown
        Entity targetId;
        bool isAttacking;
    
        AttackComponent(float rng = 100.0f, float dmg = 10.0f, float cd = 1.0f) 
            : range(rng), damage(dmg), cooldown(cd), timer(0.0f), targetId(0), isAttacking(false) {}

        static constexpr const char* Name() { return "Attack"; }
        static constexpr const char* XmlTag() { return "AttackComponent"; }

        static void LoadXml(AttackComponent& c, tinyxml2::XMLElement* el);
        static void Inspect(AttackComponent& c, Entity e);
        static void BindLua(sol::usertype<AttackComponent>& ut);
        static void SetFromLua(AttackComponent& c, sol::object v);
    };
}
