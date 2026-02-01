#include "Components/StorageComponent.h"
#include "Core/ComponentRegistry.h"
#include "imgui.h"

namespace Elysium {
    void StorageComponent::LoadXml(StorageComponent& c, tinyxml2::XMLElement* el) {
        // No XML
    }

    void StorageComponent::Inspect(StorageComponent& c, Entity e) {
        ImGui::Checkbox("Accept Deposits", &c.canAcceptDeposits);
        if(ImGui::TreeNode("Resources")) {
            for(auto& [type, amt] : c.resources) {
                ImGui::Text("%s: %.1f", type.c_str(), amt);
            }
            ImGui::TreePop();
        }
    }

    void StorageComponent::BindLua(sol::usertype<StorageComponent>& ut) {
        ut["canAcceptDeposits"] = &StorageComponent::canAcceptDeposits;
        ut["GetResource"] = [](StorageComponent& s, const std::string& type) -> float {
            auto it = s.resources.find(type);
            return it != s.resources.end() ? it->second : 0.0f;
        };
        ut["AddResource"] = [](StorageComponent& s, const std::string& type, float amt) {
            s.resources[type] += amt;
        };
    }

    void StorageComponent::SetFromLua(StorageComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.canAcceptDeposits = t.get_or("canAcceptDeposits", c.canAcceptDeposits);
        }
    }

    REGISTER_COMPONENT(StorageComponent);
}
