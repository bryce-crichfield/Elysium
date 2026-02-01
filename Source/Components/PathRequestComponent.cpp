#include "Components/PathRequestComponent.h"
#include "Core/ComponentRegistry.h"
#include "imgui.h"

namespace Elysium {
    void PathRequestComponent::LoadXml(PathRequestComponent& c, tinyxml2::XMLElement* el) {
        // Not XML loaded
    }

    void PathRequestComponent::Inspect(PathRequestComponent& c, Entity e) {
        auto Label = [](const char* label) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(label);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1);
        };
        
        Label("Target X: ");
        ImGui::DragFloat("##TargetX", &c.target.x);
        Label("Target Y: ");
        ImGui::DragFloat("##TargetY", &c.target.y);
    }

    void PathRequestComponent::BindLua(sol::usertype<PathRequestComponent>& ut) {
        ut["target"] = &PathRequestComponent::target;
    }

    void PathRequestComponent::SetFromLua(PathRequestComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            if (t["target"].valid()) {
                sol::table tgt = t["target"];
                c.target.x = tgt.get_or("x", c.target.x);
                c.target.y = tgt.get_or("y", c.target.y);
            }
        }
    }

    REGISTER_COMPONENT(PathRequestComponent);
}
