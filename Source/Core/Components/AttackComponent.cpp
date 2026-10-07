#include "Core/Components/AttackComponent.h"
#include "Core/ComponentRegistry.h"

namespace Elysium {
    void AttackComponent::LoadXml(AttackComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services) {
        c.range = el->FloatAttribute("range", 100.0f);
        c.damage = el->FloatAttribute("damage", 10.0f);
        c.cooldown = el->FloatAttribute("cooldown", 1.0f);
    }

    FieldList AttackComponent::Fields() {
        return {
            Field("Range", &AttackComponent::range, "range").Range(0.0f, 100000.0f),
            Field("Damage", &AttackComponent::damage, "damage").Range(0.0f, 100000.0f),
            Field("Cooldown", &AttackComponent::cooldown, "cooldown").Speed(0.1f).Range(0.0f, 1000.0f),
            Field("Timer", &AttackComponent::timer).Section("State"),
            Field("Is Attacking", &AttackComponent::isAttacking).Section("State"),
        };
    }

    void AttackComponent::BindLua(sol::usertype<AttackComponent>& ut) {
        ut["damage"] = &AttackComponent::damage;
        ut["range"] = &AttackComponent::range;
        ut["cooldown"] = &AttackComponent::cooldown;
        ut["timer"] = &AttackComponent::timer;
        ut["isAttacking"] = &AttackComponent::isAttacking;
        ut["targetId"] = &AttackComponent::targetId;
    }

    void AttackComponent::SetFromLua(AttackComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.damage = t.get_or("damage", c.damage);
            c.range = t.get_or("range", c.range);
            c.cooldown = t.get_or("cooldown", c.cooldown);
            c.timer = t.get_or("timer", c.timer);
            c.isAttacking = t.get_or("isAttacking", c.isAttacking);
            c.targetId = t.get_or("targetId", c.targetId);
        }
    }

    REGISTER_COMPONENT(AttackComponent);
}
