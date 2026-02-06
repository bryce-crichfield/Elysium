#include "Components/ResourceComponent.h"
#include "Core/ComponentRegistry.h"
#include "imgui.h"

namespace Elysium {
    void ResourceComponent::LoadXml(ResourceComponent& c, tinyxml2::XMLElement* el) {
        // No XML
    }

    void ResourceComponent::Inspect(ResourceComponent& c, Entity e) {
        static char typeBuffer[256];
        strncpy(typeBuffer, c.resourceType.c_str(), sizeof(typeBuffer) - 1);
        typeBuffer[sizeof(typeBuffer) - 1] = '\0';
        
        ImGui::InputText("Type", typeBuffer, sizeof(typeBuffer));
        c.resourceType = std::string(typeBuffer);
        
        ImGui::DragFloat("Amount", &c.amount);
        ImGui::DragFloat("Max", &c.maxAmount);
        ImGui::DragFloat("Rate", &c.gatherRate);
    }

    void ResourceComponent::BindLua(sol::usertype<ResourceComponent>& ut) {
        ut["resourceType"] = &ResourceComponent::resourceType;
        ut["amount"] = &ResourceComponent::amount;
        ut["maxAmount"] = &ResourceComponent::maxAmount;
        ut["gatherRate"] = &ResourceComponent::gatherRate;
    }

    void ResourceComponent::SetFromLua(ResourceComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.resourceType = t.get_or("resourceType", c.resourceType);
            c.amount = t.get_or("amount", c.amount);
            c.maxAmount = t.get_or("maxAmount", c.maxAmount);
            c.gatherRate = t.get_or("gatherRate", c.gatherRate);
        }
    }

    REGISTER_COMPONENT(ResourceComponent);
}
