#include "Components/CarryComponent.h"
#include "Core/ComponentRegistry.h"
#include "imgui.h"

namespace Elysium {
    void CarryComponent::LoadXml(CarryComponent& c, tinyxml2::XMLElement* el) {
        // No XML
    }

    void CarryComponent::Inspect(CarryComponent& c, Entity e) {
        ImGui::DragFloat("Amount", &c.amount);
        ImGui::DragFloat("Capacity", &c.capacity);
        ImGui::Text("Type: %s", c.resourceType.c_str());
    }

    void CarryComponent::BindLua(sol::usertype<CarryComponent>& ut) {
        ut["resourceType"] = &CarryComponent::resourceType;
        ut["amount"] = &CarryComponent::amount;
        ut["capacity"] = &CarryComponent::capacity;
    }

    void CarryComponent::SetFromLua(CarryComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.resourceType = t.get_or("resourceType", c.resourceType);
            c.amount = t.get_or("amount", c.amount);
            c.capacity = t.get_or("capacity", c.capacity);
        }
    }

    REGISTER_COMPONENT(CarryComponent);
}
