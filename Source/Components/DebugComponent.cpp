#include "Components/DebugComponent.h"
#include "Core/ComponentRegistry.h"
#include "imgui.h"

namespace Elysium {

    void DebugComponent::LoadXml(DebugComponent& c, tinyxml2::XMLElement* el) {}

    void DebugComponent::Inspect(DebugComponent& c, Entity e) {
        auto Label = [](const char* label) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text("%s", label);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1);
        };

        Label("Selected:");
        std::string checkId = "##DebugSelected_" + std::to_string(e);
        ImGui::Checkbox(checkId.c_str(), &c.isSelected);

        Label("Highlight:");
        std::string colorId = "##DebugColor_" + std::to_string(e);
        float col[4] = {
            c.highlightColor.r / 255.0f,
            c.highlightColor.g / 255.0f,
            c.highlightColor.b / 255.0f,
            c.highlightColor.a / 255.0f
        };
        if (ImGui::ColorEdit4(colorId.c_str(), col)) {
            c.highlightColor = {
                (unsigned char)(col[0] * 255),
                (unsigned char)(col[1] * 255),
                (unsigned char)(col[2] * 255),
                (unsigned char)(col[3] * 255)
            };
        }

        Label("AABB Bounds");
        ImGui::Text("Min: (%.2f, %.2f)", c.aabbMin.x, c.aabbMin.y);
        ImGui::Text("Max: (%.2f, %.2f)", c.aabbMax.x, c.aabbMax.y);
    }
    void DebugComponent::BindLua(sol::usertype<DebugComponent>& ut) {}
    void DebugComponent::SetFromLua(DebugComponent& c, sol::object v) {}

    REGISTER_COMPONENT(DebugComponent);
}
