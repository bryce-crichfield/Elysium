#include "Components/LayerComponent.h"
#include "Core/ComponentRegistry.h"
#include "imgui.h"
#include "tinyxml2.h"

namespace Elysium {
    LayerComponent::LayerComponent(const std::string& name) 
        : name(name) {}

    void LayerComponent::LoadXml(LayerComponent& c, tinyxml2::XMLElement* el) {
        const char* name = el->Attribute("name");
        c.name = name ? name : "default";
    }

    void LayerComponent::SaveXml(const LayerComponent& c, XMLBuilder& builder) {
        builder.AddElement("LayerComponent")
            .SetAttribute("name", c.name.c_str());
    }

    void LayerComponent::Inspect(LayerComponent& c, Entity e) {
        auto Label = [](const char* label) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(label);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1);
        };

        static char nameBuffer[256];
        strncpy(nameBuffer, c.name.c_str(), sizeof(nameBuffer) - 1);
        nameBuffer[sizeof(nameBuffer) - 1] = '\0';

        Label("Name: ");
        if (ImGui::InputText("##Name", nameBuffer, sizeof(nameBuffer))) {
            c.name = std::string(nameBuffer);
        }
    }

    void LayerComponent::BindLua(sol::usertype<LayerComponent>& ut) {
        ut["name"] = &LayerComponent::name;
    }

    void LayerComponent::SetFromLua(LayerComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.name = t.get_or("name", c.name);
        }
    }

    REGISTER_COMPONENT(LayerComponent);
}
