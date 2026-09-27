#include "Components/PrefabInstanceComponent.h"
#include "Core/ComponentRegistry.h"
#include "Core/PrefabInstance.h"
#include "Core/World.h"
#include "Editor/PrefabEditing.h"
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
        World* world = editor.GetWorld();
        if (ImGui::Button(ICON_FA_PEN_TO_SQUARE "  Open Prefab")) editor.OpenPrefab(c.FullPath());
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_BOX_OPEN "  Unpack") && world) {
            // Last: this component is gone afterwards.
            PrefabEditing::Unpack(world, e);
            return;
        }
        ItemTooltip("Turn this placement into plain entities that no longer follow the prefab");

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

            // Typed by the field it drives; a field without a type is edited as text.
            ImGui::PushID(param.name.c_str());
            PropertyLabel(param.name.c_str());
            bool changed = false;
            if (const FieldInfo* field = ComponentRegistry::Instance().FindField(param.component, param.field)) {
                changed = InspectFieldText("##value", *field, value);
            } else {
                char buffer[256];
                snprintf(buffer, sizeof(buffer), "%s", value.c_str());
                changed = ImGui::InputText("##value", buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue);
                if (changed) value = buffer;
            }
            if (changed) {
                for (const auto& target : parameters) {
                    if (target.name == param.name) {
                        PrefabInstances::ApplyOverride(world, ids, {target.entity, target.component, target.field, value}, services);
                    }
                }
            }
            ItemTooltip((param.component + "." + param.field).c_str());
            ImGui::PopID();
        }
    }

    REGISTER_COMPONENT(PrefabInstanceComponent);
}
