#include "InspectorEditor.h"
#include <cstring>
#include <algorithm>
#include <set>
#include "Components/NameComponent.h"
#include "Components/PrefabInstanceComponent.h"
#include "Core/ComponentRegistry.h"
#include "Core/Prefab.h"
#include "Core/Scene.h"
#include "Core/World.h"
#include "Core/Common.h"
#include "Editor/Widgets.h"
#include "Interfaces/IEditorService.h"

namespace Elysium {

using namespace Services;

InspectorEditor::InspectorEditor(ServiceLocator& services) : Editor(services, Title) {}

Entity InspectorEditor::GetPrimarySelection(IEditorService& service) {
    const auto& selected = service.GetSelectedEntities();
    return selected.empty() ? INVALID_ENTITY : selected.back();
}

void InspectorEditor::Draw() {
    Profile;

    auto& service = services_.Get<IEditorService>();
    auto* world = service.GetWorld();
    const Entity entity = GetPrimarySelection(service);

    if (BeginWindow()) {
        if (world) DrawPrefabParameters(service, entity);

        if (!world) {
            EmptyState("No world loaded");
        } else if (entity == INVALID_ENTITY) {
            EmptyState("Select an entity to inspect");
        } else {
            DrawHeader(service, entity);

            componentToRemove_.clear();
            for (const auto& placeholder : service.GetComponentPlaceholders()) {
                if (placeholder.hasComponentFunc(entity, world)) DrawComponent(service, entity, placeholder);
            }
            if (!componentToRemove_.empty()) {
                for (const auto& placeholder : service.GetComponentPlaceholders()) {
                    if (placeholder.name == componentToRemove_) {
                        placeholder.removeComponentFunc(entity, world);
                        break;
                    }
                }
            }

            DrawAddComponent(service, entity);
            openRequest_.reset();
        }
    }
    EndWindow();
}

void InspectorEditor::DrawPrefabParameters(IEditorService& service, Entity selected) {
    const int active = service.GetActiveDocument();
    if (active < 0) return;
    EditorDocument& doc = *service.GetDocuments()[active];
    World* world = doc.scene->GetWorld();

    if (!CollapsingSection(ICON_FA_BOX "  Prefab Parameters", true, ImGuiTreeNodeFlags_DefaultOpen)) return;
    BeginSectionBody();

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
        ColoredText(target == INVALID_ENTITY ? Palette().Error : Palette().TextMuted, targetLabel.c_str());
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
        for (const auto& [component, support] : ComponentRegistry::Instance().GetPrefabFieldSupport()) {
            tinyxml2::XMLDocument scratch;
            tinyxml2::XMLElement* el = support.serialize(scratch, world, selected);
            if (!el || !el->FirstAttribute()) continue;
            if (!ImGui::BeginMenu(component.c_str())) continue;
            for (const tinyxml2::XMLAttribute* attr = el->FirstAttribute(); attr; attr = attr->Next()) {
                if (!ImGui::MenuItem(attr->Name())) continue;

                // The entity needs a stable local id to be addressed from placements.
                auto idIt = doc.localIds.find(selected);
                if (idIt == doc.localIds.end()) {
                    int next = 0;
                    for (const auto& [e, id] : doc.localIds) next = std::max(next, id + 1);
                    idIt = doc.localIds.emplace(selected, next).first;
                }

                // Default name: the field, capitalized, made unique.
                std::string base = attr->Name();
                if (!base.empty()) base[0] = (char)toupper((unsigned char)base[0]);
                std::set<std::string> taken;
                for (const auto& p : doc.parameters) taken.insert(p.name);
                std::string name = base;
                for (int n = 2; taken.count(name); ++n) name = base + std::to_string(n);

                doc.parameters.push_back({name, idIt->second, component, attr->Name()});
            }
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }

    EndSectionBody();
    ImGui::Separator();
}

void InspectorEditor::DrawHeader(IEditorService& service, Entity entity) {
    auto* world = service.GetWorld();

    if (nameBufferEntity_ != entity) {
        strncpy(nameBuffer_, world->GetEntityName(entity).c_str(), sizeof(nameBuffer_) - 1);
        nameBuffer_[sizeof(nameBuffer_) - 1] = '\0';
        nameBufferEntity_ = entity;
    }

    const std::string id = "#" + std::to_string(entity);
    ImGui::AlignTextToFramePadding();
    ColoredText(Palette().Accent, ICON_FA_CUBE);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-(ImGui::CalcTextSize(id.c_str()).x + ImGui::GetStyle().ItemSpacing.x + ExpandCollapseWidth()));
    if (ImGui::InputTextWithHint("##EntityName", "Unnamed entity", nameBuffer_, sizeof(nameBuffer_))) {
        if (world->HasComponent<NameComponent>(entity)) {
            world->GetComponent<NameComponent>(entity).name = nameBuffer_;
        } else {
            world->AddComponent(entity, NameComponent(nameBuffer_));
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", id.c_str());
    ImGui::SameLine();
    ExpandCollapseButtons(openRequest_);
    ImGui::Spacing();
}

void InspectorEditor::DrawComponent(IEditorService& service, Entity entity, const ComponentPlaceholder& placeholder) {
    auto* world = service.GetWorld();
    ImGui::PushID(placeholder.name.c_str());

    ApplyOpenRequest(openRequest_);
    const bool open = CollapsingSection(placeholder.name.c_str(), true);

    // Per-component actions behind a kebab button on the header's right edge.
    AlignRight(ButtonWidth(ICON_FA_ELLIPSIS_VERTICAL));
    if (IconButton(ICON_FA_ELLIPSIS_VERTICAL, "Component actions")) ImGui::OpenPopup("ComponentActions");
    if (ImGui::BeginPopup("ComponentActions")) {
        if (ImGui::MenuItem(ICON_FA_ROTATE_LEFT "  Reset")) placeholder.resetComponentFunc(entity, world);
        ImGui::PushStyleColor(ImGuiCol_Text, Palette().Error);
        if (ImGui::MenuItem(ICON_FA_TRASH_CAN "  Remove")) componentToRemove_ = placeholder.name;
        ImGui::PopStyleColor();
        ImGui::EndPopup();
    }

    if (open) {
        BeginSectionBody();
        placeholder.drawFunc(entity, world);
        EndSectionBody();
    }
    ImGui::PopID();
}

void InspectorEditor::DrawAddComponent(IEditorService& service, Entity entity) {
    auto* world = service.GetWorld();

    ImGui::Spacing();
    if (ImGui::Button(ICON_FA_PLUS "  Add Component", ImVec2(-FLT_MIN, 0))) {
        componentSearch_[0] = '\0';
        ImGui::OpenPopup("AddComponent");
    }

    ImGui::SetNextWindowSizeConstraints(ImVec2(ImGui::GetItemRectSize().x, 0), ImVec2(FLT_MAX, ImGui::GetFontSize() * 20));
    if (!ImGui::BeginPopup("AddComponent")) return;

    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    SearchField("##ComponentSearch", componentSearch_, sizeof(componentSearch_));
    ImGui::Separator();

    for (const auto& placeholder : service.GetComponentPlaceholders()) {
        if (placeholder.hasComponentFunc(entity, world)) continue;
        if (!MatchesSearch(placeholder.name, componentSearch_)) continue;

        if (ImGui::Selectable(placeholder.name.c_str())) {
            placeholder.addComponentFunc(entity, world);
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::EndPopup();
}

}  // namespace Elysium
