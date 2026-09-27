#include "Components/PrefabInstanceComponent.h"
#include "Core/ComponentRegistry.h"
#include "Core/PrefabInstance.h"
#include "Core/World.h"
#include "Editor/Widgets.h"
#include "Interfaces/IAssetService.h"
#include "Interfaces/IEditorService.h"
#include "imgui.h"
#include <set>

namespace Elysium {
    void PrefabInstanceComponent::Inspect(PrefabInstanceComponent& c, Entity e, ServiceLocator& services) {
        auto& editor = services.Get<Services::IEditorService>();

        ReadOnlyRow("Source", c.src.c_str());
        ReadOnlyRow("Instance", c.instanceId.c_str());
        if (ImGui::Button(ICON_FA_PEN_TO_SQUARE "  Open Prefab")) editor.OpenPrefab(c.FullPath());

        World* world = editor.GetWorld();
        const Prefab* prefab = Prefab::Get(services.Get<Services::IAssetService>(), c.FullPath());
        if (!world || !prefab || prefab->GetParameters().empty()) return;
        const auto& parameters = prefab->GetParameters();

        // The instance's entities by local id, to route each parameter to its target.
        PrefabIdMap ids;
        world->Query<PrefabInstanceComponent>([&](Entity entity, PrefabInstanceComponent& tag) {
            if (tag.instanceId == c.instanceId && tag.localEntityId >= 0) ids[tag.localEntityId] = entity;
        });

        SectionHeader("Parameters");
        std::set<std::string> shown;
        for (const auto& param : parameters) {
            auto it = ids.find(param.entity);
            if (it == ids.end() || !shown.insert(param.name).second) continue;

            std::string value = PrefabInstances::ReadField(world, it->second, param.component, param.field);
            char buffer[256];
            snprintf(buffer, sizeof(buffer), "%s", value.c_str());

            ImGui::PushID(param.name.c_str());
            PropertyLabel(param.name.c_str());
            if (ImGui::InputText("##value", buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
                for (const auto& target : parameters) {
                    if (target.name == param.name) {
                        PrefabInstances::ApplyOverride(world, ids, {target.entity, target.component, target.field, buffer}, services);
                    }
                }
            }
            ItemTooltip((param.component + "." + param.field + "  (Enter to apply)").c_str());
            ImGui::PopID();
        }
    }

    REGISTER_COMPONENT(PrefabInstanceComponent);
}
