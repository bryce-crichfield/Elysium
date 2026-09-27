#include "InspectorEditor.h"
#include <cstring>
#include <algorithm>
#include <set>
#include "Components/NameComponent.h"
#include "Components/PrefabInstanceComponent.h"
#include "Core/ComponentRegistry.h"
#include "Core/Prefab.h"
#include "Core/PrefabInstance.h"
#include "Core/Scene.h"
#include "Core/World.h"
#include "Core/Common.h"
#include "Editor/AssetStyle.h"
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
        // Only scenes and prefabs have entities; other tabs leave this panel dark.
        if (const EditorDocument* doc = service.GetActiveDocumentInfo(); doc && !doc->HasWorld()) {
            UnavailableState((std::string(StyleOf(doc->kind).label) + "s have no entities").c_str());
        } else if (!world) {
            EmptyState("No world loaded");
        } else if (entity == INVALID_ENTITY) {
            EmptyState("Select an entity to inspect");
        } else {
            DrawHeader(service, entity);

            // A placed prefab is a black box: only what the placement owns (its name and
            // transform) and the prefab's exposed parameters are editable here.
            const bool placement = PrefabInstances::IsRoot(*world, entity);
            auto& registry = ComponentRegistry::Instance();

            componentToRemove_.clear();
            for (const auto& placeholder : service.GetComponentPlaceholders()) {
                if (!placeholder.hasComponentFunc(entity, world)) continue;
                const bool isTag = placeholder.name == PrefabInstanceComponent::Name();
                if (placement && !isTag && !registry.IsPlacementOwned(placeholder.name)) continue;
                DrawComponent(service, entity, placeholder, !placement);
            }
            if (!componentToRemove_.empty()) {
                for (const auto& placeholder : service.GetComponentPlaceholders()) {
                    if (placeholder.name == componentToRemove_) {
                        placeholder.removeComponentFunc(entity, world);
                        break;
                    }
                }
            }

            if (placement) {
                ImGui::Spacing();
                MutedText("The rest of this entity belongs to its prefab: open the prefab to change it, expose a "
                          "parameter for it, or unpack this placement.");
            } else {
                DrawAddComponent(service, entity);
            }
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

void InspectorEditor::DrawComponent(IEditorService& service, Entity entity, const ComponentPlaceholder& placeholder,
                                    bool removable) {
    auto* world = service.GetWorld();
    ImGui::PushID(placeholder.name.c_str());

    ApplyOpenRequest(openRequest_);
    const bool open = CollapsingSection(placeholder.name.c_str(), true);

    // Per-component actions (not on a placed prefab, whose components are its prefab's).
    if (removable) {
        AlignRight(ButtonWidth(ICON_FA_ELLIPSIS_VERTICAL));
        if (IconButton(ICON_FA_ELLIPSIS_VERTICAL, "Component actions")) ImGui::OpenPopup("ComponentActions");
        if (ImGui::BeginPopup("ComponentActions")) {
            if (ImGui::MenuItem(ICON_FA_ROTATE_LEFT "  Reset")) placeholder.resetComponentFunc(entity, world);
            ImGui::PushStyleColor(ImGuiCol_Text, Palette().Error);
            if (ImGui::MenuItem(ICON_FA_TRASH_CAN "  Remove")) componentToRemove_ = placeholder.name;
            ImGui::PopStyleColor();
            ImGui::EndPopup();
        }
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
