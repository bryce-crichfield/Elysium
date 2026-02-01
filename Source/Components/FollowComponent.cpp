#include "Components/FollowComponent.h"
#include "Core/ComponentRegistry.h"
#include "imgui.h"

namespace Elysium {
    void FollowComponent::LoadXml(FollowComponent& c, tinyxml2::XMLElement* el) {
        // Attributes not standard in loader
    }

    void FollowComponent::Inspect(FollowComponent& c, Entity e) {
        auto Label = [](const char* label) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(label);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1);
        };

        static char targetBuffer[256];
        strncpy(targetBuffer, c.targetEntityName.c_str(), sizeof(targetBuffer) - 1);
        targetBuffer[sizeof(targetBuffer) - 1] = '\0';

        Label("Follow Speed: ");
        ImGui::DragFloat("##FollowSpeed", &c.speed, 0.1f, 0.0f);

        Label("Target: ");
        if (ImGui::InputText("##TargetEntityName", targetBuffer, sizeof(targetBuffer))) {
            c.targetEntityName = std::string(targetBuffer);
        }
    }

    void FollowComponent::BindLua(sol::usertype<FollowComponent>& ut) {
        ut["speed"] = &FollowComponent::speed;
        ut["targetEntityName"] = &FollowComponent::targetEntityName;
    }

    void FollowComponent::SetFromLua(FollowComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.speed = t.get_or("speed", c.speed);
            c.targetEntityName = t.get_or("targetEntityName", c.targetEntityName);
        }
    }

    REGISTER_COMPONENT(FollowComponent);
}
