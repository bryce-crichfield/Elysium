#include "Components/LocationComponent.h"
#include "Core/ComponentRegistry.h"
#include "imgui.h"

namespace Elysium {
    LocationComponent::LocationComponent(int x, int y) : x(x), y(y) {}

    void LocationComponent::LoadXml(LocationComponent& c, tinyxml2::XMLElement* el) {
        c.x = el->IntAttribute("x", 0);
        c.y = el->IntAttribute("y", 0);
    }

    void LocationComponent::Inspect(LocationComponent& c, Entity e) {
        auto Label = [](const char* label) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(label);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1);
        };

        Label("X: ");
        ImGui::DragInt("##X", &c.x);
        Label("Y: ");
        ImGui::DragInt("##Y", &c.y);
    }

    void LocationComponent::BindLua(sol::usertype<LocationComponent>& ut) {
        ut["x"] = &LocationComponent::x;
        ut["y"] = &LocationComponent::y;
    }

    void LocationComponent::SetFromLua(LocationComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.x = t.get_or("x", c.x);
            c.y = t.get_or("y", c.y);
        }
    }

    REGISTER_COMPONENT(LocationComponent);
}
