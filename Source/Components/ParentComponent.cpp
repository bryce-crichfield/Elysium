#include "Components/ParentComponent.h"
#include "Core/ComponentRegistry.h"
#include "imgui.h"

namespace Elysium {

    void ParentComponent::LoadXml(ParentComponent& c, tinyxml2::XMLElement* el) {
        const char* target = el->Attribute("target");
        c.targetName = target ? target : "";
        // parent (Entity ID) is populated later by the hierarchy resolution pass
    }

    void ParentComponent::SaveXml(const ParentComponent& c, XMLBuilder& builder) {
        if (!c.targetName.empty()) {
            builder.AddElement("ParentComponent")
                .SetAttribute("target", c.targetName.c_str());
        }
    }

    void ParentComponent::Inspect(ParentComponent& c, Entity e) {
        auto Label = [](const char* label) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(label);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1);
        };

        Label("Parent: ");
        ImGui::Text("%s (id=%zu)", c.targetName.c_str(), c.parent);

        Label("Child Index: ");
        ImGui::Text("%u", c.childIndex);
    }

    void ParentComponent::BindLua(sol::usertype<ParentComponent>& ut) {
        ut["parent"]     = &ParentComponent::parent;
        ut["targetName"] = &ParentComponent::targetName;
    }

    REGISTER_COMPONENT(ParentComponent);
}
