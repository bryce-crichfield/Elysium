#include "InspectorEditor.h"
#include <cstring>
#include "Components/NameComponent.h"
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

void InspectorEditor::DrawHeader(IEditorService& service, Entity entity) {
    auto* world = service.GetWorld();

    if (nameBufferEntity_ != entity) {
        strncpy(nameBuffer_, world->GetEntityName(entity).c_str(), sizeof(nameBuffer_) - 1);
        nameBuffer_[sizeof(nameBuffer_) - 1] = '\0';
        nameBufferEntity_ = entity;
    }

    const std::string id = "#" + std::to_string(entity);
    ImGui::AlignTextToFramePadding();
    ColoredText(Palette::Accent, ICON_FA_CUBE);
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
    const bool open = CollapsingSection(placeholder.name.c_str(), true, ImGuiTreeNodeFlags_DefaultOpen);

    // Per-component actions behind a kebab button on the header's right edge.
    AlignRight(ButtonWidth(ICON_FA_ELLIPSIS_VERTICAL));
    if (IconButton(ICON_FA_ELLIPSIS_VERTICAL, "Component actions")) ImGui::OpenPopup("ComponentActions");
    if (ImGui::BeginPopup("ComponentActions")) {
        if (ImGui::MenuItem(ICON_FA_ROTATE_LEFT "  Reset")) placeholder.resetComponentFunc(entity, world);
        ImGui::PushStyleColor(ImGuiCol_Text, Palette::Error);
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
