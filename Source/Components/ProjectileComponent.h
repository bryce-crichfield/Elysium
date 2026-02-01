#pragma once
#include "Core/Component.h"
#include "Core/Entity.h"

namespace Elysium {
    struct ProjectileComponent {
        float damage;
        Entity targetId;
        float speed;
        float lifetime; // seconds before auto-destroy

        ProjectileComponent(float dmg = 10.0f, Entity target = 0, float spd = 300.0f)
            : damage(dmg), targetId(target), speed(spd), lifetime(5.0f) {}

        static constexpr const char* Name() { return "Projectile"; }
        static constexpr const char* XmlTag() { return "ProjectileComponent"; }

        static void LoadXml(ProjectileComponent& c, tinyxml2::XMLElement* el);
        static void Inspect(ProjectileComponent& c, Entity e);
        static void BindLua(sol::usertype<ProjectileComponent>& ut);
        static void SetFromLua(ProjectileComponent& c, sol::object v);
    };
}
