#include "Components/ProjectileComponent.h"
#include "Core/ComponentRegistry.h"
#include "imgui.h"

namespace Elysium {
    void ProjectileComponent::LoadXml(ProjectileComponent& c, tinyxml2::XMLElement* el) {
        // No XML
    }

    void ProjectileComponent::Inspect(ProjectileComponent& c, Entity e) {
        // Basic inspect
        ImGui::DragFloat("Damage", &c.damage);
        ImGui::DragFloat("Speed", &c.speed);
        ImGui::DragFloat("Lifetime", &c.lifetime);
    }

    void ProjectileComponent::BindLua(sol::usertype<ProjectileComponent>& ut) {
        ut["damage"] = &ProjectileComponent::damage;
        ut["targetId"] = &ProjectileComponent::targetId;
        ut["speed"] = &ProjectileComponent::speed;
        ut["lifetime"] = &ProjectileComponent::lifetime;
    }

    void ProjectileComponent::SetFromLua(ProjectileComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.damage = t.get_or("damage", c.damage);
            c.speed = t.get_or("speed", c.speed);
            c.lifetime = t.get_or("lifetime", c.lifetime);
            c.targetId = t.get_or("targetId", c.targetId);
        }
    }

    REGISTER_COMPONENT(ProjectileComponent);
}
