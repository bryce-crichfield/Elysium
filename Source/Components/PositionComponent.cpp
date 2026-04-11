#include "Components/PositionComponent.h"
#include "Core/ComponentRegistry.h"
#include "imgui.h"

namespace Elysium {
    PositionComponent::PositionComponent(float x, float y) : x(x), y(y) {}

    void PositionComponent::LoadXml(PositionComponent& c, tinyxml2::XMLElement* el) {
        c.x = el->FloatAttribute("x", 0.0f);
        c.y = el->FloatAttribute("y", 0.0f);
    }

    void PositionComponent::SaveXml(const PositionComponent& c, XMLBuilder& builder) {
        builder.AddElement("PositionComponent")
            .SetAttribute("x", c.x)
            .SetAttribute("y", c.y);
    }

    void PositionComponent::Inspect(PositionComponent& c, Entity e) {
        auto Label = [](const char* label) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(label);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1);
        };

        Label("X: ");
        std::string xId = "##PosX_" + std::to_string(e);
        ImGui::DragFloat(xId.c_str(), &c.x, 1.0f);

        Label("Y: ");
        std::string yId = "##PosY_" + std::to_string(e);
        ImGui::DragFloat(yId.c_str(), &c.y, 1.0f);
    }

    void PositionComponent::BindLua(sol::usertype<PositionComponent>& ut) {
         ut["x"] = &PositionComponent::x;
         ut["y"] = &PositionComponent::y;
    }

    void PositionComponent::SetFromLua(PositionComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.x = t.get_or("x", c.x);
            c.y = t.get_or("y", c.y);
        }
    }
    
    REGISTER_COMPONENT(PositionComponent);
}
