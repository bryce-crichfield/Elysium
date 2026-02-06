#include "Components/KinematicsComponent.h"
#include "Core/ComponentRegistry.h"
#include "imgui.h"

namespace Elysium {
    void KinematicsComponent::LoadXml(KinematicsComponent& c, tinyxml2::XMLElement* el) {
        c.friction = el->FloatAttribute("friction", 5.0f);
        c.maxSpeed = el->FloatAttribute("maxSpeed", 200.0f);
    }

    void KinematicsComponent::Inspect(KinematicsComponent& c, Entity e) {
        auto Label = [](const char* label) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(label);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1);
        };
        
        Label("Max Speed: ");
        ImGui::DragFloat("##MaxSpeed", &c.maxSpeed, 1.0f, 0.0f);
        Label("Friction: ");
        ImGui::DragFloat("##Friction", &c.friction, 0.1f, 0.0f);
        Label("Velocity X: ");
        ImGui::DragFloat("##VelX", &c.velocity.x);
        Label("Velocity Y: ");
        ImGui::DragFloat("##VelY", &c.velocity.y);
    }

    void KinematicsComponent::BindLua(sol::usertype<KinematicsComponent>& ut) {
        ut["maxSpeed"] = &KinematicsComponent::maxSpeed;
        ut["friction"] = &KinematicsComponent::friction;
        ut["velocity"] = &KinematicsComponent::velocity;
        ut["acceleration"] = &KinematicsComponent::acceleration;
    }

    void KinematicsComponent::SetFromLua(KinematicsComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.maxSpeed = t.get_or("maxSpeed", c.maxSpeed);
            c.friction = t.get_or("friction", c.friction);
            if (t["velocity"].valid()) {
                sol::table vt = t["velocity"];
                c.velocity.x = vt.get_or("x", c.velocity.x);
                c.velocity.y = vt.get_or("y", c.velocity.y);
            }
            if (t["acceleration"].valid()) {
                sol::table at = t["acceleration"];
                c.acceleration.x = at.get_or("x", c.acceleration.x);
                c.acceleration.y = at.get_or("y", c.acceleration.y);
            }
        }
    }

    REGISTER_COMPONENT(KinematicsComponent);
}
