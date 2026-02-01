#include "Components/ScaleComponent.h"
#include "Core/ComponentRegistry.h"
#include "imgui.h"

namespace Elysium {
    void ScaleComponent::LoadXml(ScaleComponent& c, tinyxml2::XMLElement* el) {
        c.x = el->FloatAttribute("x", 1.0f);
        c.y = el->FloatAttribute("y", 1.0f);
    }

    void ScaleComponent::Inspect(ScaleComponent& c, Entity e) {
        auto Label = [](const char* label) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(label);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1);
        };

        Label("X: ");
        std::string xId = "##ScaleX_" + std::to_string(e);
        ImGui::DragFloat(xId.c_str(), &c.x, 0.01f, 0.0f);

        Label("Y: ");
        std::string yId = "##ScaleY_" + std::to_string(e);
        ImGui::DragFloat(yId.c_str(), &c.y, 0.01f, 0.0f);
    }

    void ScaleComponent::BindLua(sol::usertype<ScaleComponent>& ut) {
        ut["x"] = &ScaleComponent::x;
        ut["y"] = &ScaleComponent::y;
    }

    void ScaleComponent::SetFromLua(ScaleComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.x = t.get_or("x", c.x);
            c.y = t.get_or("y", c.y);
        }
    }

    REGISTER_COMPONENT(ScaleComponent);
}
