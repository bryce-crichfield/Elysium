#include "Components/FollowComponent.h"
#include "Core/ComponentRegistry.h"
#include "imgui.h"

namespace Elysium {
    void FollowComponent::LoadXml(FollowComponent& c, tinyxml2::XMLElement* el) {
        el->QueryFloatAttribute("followSpeed", &c.speed);
        el->QueryFloatAttribute("offsetX", &c.offsetX);
        el->QueryFloatAttribute("offsetY", &c.offsetY);
    }

    void FollowComponent::Inspect(FollowComponent& c, Entity e) {
        auto Label = [](const char* label) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(label);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1);
        };

        Label("Follow Speed: ");
        ImGui::DragFloat("##FollowSpeed", &c.speed, 0.1f);
        Label("Offset X: ");
        ImGui::DragFloat("##OffsetX", &c.offsetX, 1.0f);
        Label("Offset Y: ");
        ImGui::DragFloat("##OffsetY", &c.offsetY, 1.0f);
    }

    void FollowComponent::BindLua(sol::usertype<FollowComponent>& ut) {
        ut["speed"]   = &FollowComponent::speed;
        ut["offsetX"] = &FollowComponent::offsetX;
        ut["offsetY"] = &FollowComponent::offsetY;
    }

    void FollowComponent::SetFromLua(FollowComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.speed   = t.get_or("speed",   c.speed);
            c.offsetX = t.get_or("offsetX", c.offsetX);
            c.offsetY = t.get_or("offsetY", c.offsetY);
        }
    }

    REGISTER_COMPONENT(FollowComponent);
}
