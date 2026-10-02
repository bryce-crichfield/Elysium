#include "Editor/Settings/PrefabSettings.h"
#include "Editor/Inspectors/FieldInspector.h"
#include <algorithm>
#include <set>
#include "Core/Components/PrefabInstanceComponent.h"
#include "Core/Common.h"
#include "Core/ComponentRegistry.h"
#include "Core/Prefab.h"
#include "Core/Scene.h"
#include "Core/World.h"
#include "Editor/Style/AssetStyle.h"
#include "Editor/Widgets/Widgets.h"
#include "Interfaces/IEditorService.h"

namespace Elysium {

using namespace Services;

void PrefabSettings::Draw(EditorDocument& doc) {
    Profile;

    auto& service = services_.Get<IEditorService>();
    World* world = doc.scene->GetWorld();
    const auto& selection = service.GetSelectedEntities();
    const Entity selected = selection.empty() ? INVALID_ENTITY : selection.back();

    BeginKindSettings(AssetKind::Prefab, "PrefabSettings", doc.title + " Settings");
    SectionHeader("Parameters");

    // Entity for a local id, for labelling each parameter's target.
    auto entityOf = [&](int localId) {
        for (const auto& [entity, id] : doc.localIds) if (id == localId) return entity;
        return INVALID_ENTITY;
    };

    if (doc.parameters.empty()) MutedText("None. Select an entity, then add one of its fields.");

    int toRemove = -1;
    for (int i = 0; i < (int)doc.parameters.size(); ++i) {
        PrefabParameter& param = doc.parameters[i];
        ImGui::PushID(i);

        FieldTypeBadge(ComponentRegistry::Instance().FindField(param.component, param.field));
        ImGui::SameLine();

        // Parameters sharing a name drive all their fields together.
        char nameBuffer[128];
        snprintf(nameBuffer, sizeof(nameBuffer), "%s", param.name.c_str());
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.35f);
        if (ImGui::InputText("##name", nameBuffer, sizeof(nameBuffer), ImGuiInputTextFlags_EnterReturnsTrue) && nameBuffer[0]) {
            param.name = nameBuffer;
        }
        ItemTooltip("Parameter name (Enter to apply). Parameters sharing a name are set together.");

        ImGui::SameLine();
        const Entity target = entityOf(param.entity);
        const std::string targetLabel = (target != INVALID_ENTITY ? EntityLabel(world->GetEntityName(target), target) : "#" + std::to_string(param.entity)) +
                                        "  " + param.component + "." + param.field;
        ImGui::AlignTextToFramePadding();
        ColoredText(target == INVALID_ENTITY ? Editor::Palette().Error : Editor::Palette().TextMuted, targetLabel.c_str());
        if (target == INVALID_ENTITY) ItemTooltip("Target entity no longer exists");
        else if (ImGui::IsItemClicked()) service.SelectEntity(target);

        AlignRight(ButtonWidth(ICON_FA_TRASH_CAN));
        if (IconButton(ICON_FA_TRASH_CAN, "Remove parameter")) toRemove = i;
        ImGui::PopID();
    }
    if (toRemove >= 0) doc.parameters.erase(doc.parameters.begin() + toRemove);

    // Add: a field of the selected entity. Only the prefab's own entities (not entities of
    // prefabs placed inside it) can be exposed.
    const bool canAdd = selected != INVALID_ENTITY && !world->HasComponent<PrefabInstanceComponent>(selected);
    ImGui::BeginDisabled(!canAdd);
    if (ImGui::Button(ICON_FA_PLUS "  Add Parameter")) ImGui::OpenPopup("AddParameter");
    ImGui::EndDisabled();
    if (!canAdd) ItemTooltip("Select one of this prefab's own entities first");

    if (ImGui::BeginPopup("AddParameter")) {
        auto& registry = ComponentRegistry::Instance();
        for (const auto& [component, support] : registry.GetPrefabFieldSupport()) {
            // A typed component offers its serialized fields; an untyped one whatever
            // attributes it writes.
            std::vector<std::pair<std::string, const FieldInfo*>> choices;  // key, field
            tinyxml2::XMLDocument scratch;
            tinyxml2::XMLElement* el = support.serialize(scratch, world, selected);
            if (!el) continue;  // the entity doesn't have it
            if (const FieldList* fields = registry.GetFields(component)) {
                for (const auto& field : *fields) {
                    if (field.Serialized()) choices.emplace_back(field.key, &field);
                }
            } else {
                for (auto* attr = el->FirstAttribute(); attr; attr = attr->Next()) choices.emplace_back(attr->Name(), nullptr);
            }
            if (choices.empty() || !ImGui::BeginMenu(component.c_str())) continue;
            for (const auto& [key, field] : choices) {
                FieldTypeBadge(field);
                ImGui::SameLine();
                if (!ImGui::MenuItem(field ? field->label.c_str() : key.c_str())) continue;

                // The entity needs a stable local id to be addressed from placements.
                auto idIt = doc.localIds.find(selected);
                if (idIt == doc.localIds.end()) {
                    int next = 0;
                    for (const auto& [e, id] : doc.localIds) next = std::max(next, id + 1);
                    idIt = doc.localIds.emplace(selected, next).first;
                }

                // Default name: the field's label (or its key, capitalized), made unique.
                std::string base = field ? field->label : key;
                std::erase(base, ' ');
                if (!base.empty()) base[0] = (char)toupper((unsigned char)base[0]);
                std::set<std::string> taken;
                for (const auto& p : doc.parameters) taken.insert(p.name);
                std::string name = base;
                for (int n = 2; taken.count(name); ++n) name = base + std::to_string(n);

                doc.parameters.push_back({name, idIt->second, component, key});
            }
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }

    EndKindSettings();
}

}  // namespace Elysium
