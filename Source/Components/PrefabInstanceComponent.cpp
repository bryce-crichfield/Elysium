#include "Components/PrefabInstanceComponent.h"
#include "Core/ComponentRegistry.h"
#include "imgui.h"

namespace Elysium {

    void PrefabInstanceComponent::Inspect(PrefabInstanceComponent& c, Entity e) {
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Prefab: ");
        ImGui::SameLine(140.0f);
        ImGui::TextDisabled("%s", c.src.c_str());

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Instance: ");
        ImGui::SameLine(140.0f);
        ImGui::TextDisabled("%s", c.instanceId.c_str());

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Local Id: ");
        ImGui::SameLine(140.0f);
        ImGui::TextDisabled("%d", c.localEntityId);
    }

    REGISTER_COMPONENT(PrefabInstanceComponent);
}
