#include "InspectorEditor.h"
#include <cstring>
#include <algorithm>
#include <set>
#include "Core/Components/NameComponent.h"
#include "Core/Components/PrefabInstanceComponent.h"
#include "Core/ComponentRegistry.h"
#include "Core/EntitySerializer.h"
#include "Editor/Commands/EditorCommands.h"
#include "Core/Prefab.h"
#include "Core/PrefabInstance.h"
#include "Core/Scene.h"
#include "Core/World.h"
#include "Core/Common.h"
#include "Editor/Style/AssetStyle.h"
#include "Editor/Widgets/Widgets.h"
#include "Editor/EditorApplication.h"

namespace Elysium {

using namespace Services;

InspectorEditor::InspectorEditor(EditorApplication& editor) : Editor(editor, Title) {}

Entity InspectorEditor::GetPrimarySelection(EditorApplication& editor) {
    const auto& selected = editor.GetSelectedEntities();
    return selected.empty() ? INVALID_ENTITY : selected.back();
}

void InspectorEditor::Draw() {
    Profile;

    auto& editor = editor_;
    auto* world = editor.GetWorld();
    const Entity entity = GetPrimarySelection(editor);

    if (BeginWindow()) {
        // Only scenes and prefabs have entities; other tabs leave this panel dark.
        if (const EditorDocument* doc = editor.GetActiveDocumentInfo(); doc && !doc->HasWorld()) {
            UnavailableState((std::string(StyleOf(doc->kind).label) + "s have no entities").c_str());
        } else if (!world) {
            EmptyState("No world loaded");
        } else if (entity == INVALID_ENTITY) {
            EmptyState("Select an entity to inspect");
        } else {
            DrawHeader(editor, entity);

            // A placed prefab is a black box: only what the placement owns (its name and
            // transform) and the prefab's exposed parameters are editable here.
            const bool placement = PrefabInstances::IsRoot(*world, entity);
            auto& registry = ComponentRegistry::Instance();

            componentToRemove_.clear();
            for (const auto& placeholder : editor.GetComponentPlaceholders()) {
                if (!placeholder.hasComponentFunc(entity, world)) continue;
                const bool isTag = placeholder.name == PrefabInstanceComponent::Name();
                if (placement && !isTag && !registry.IsPlacementOwned(placeholder.name)) continue;
                DrawComponent(editor, entity, placeholder, !placement);
            }
            if (!componentToRemove_.empty()) {
                for (const auto& placeholder : editor.GetComponentPlaceholders()) {
                    if (placeholder.name == componentToRemove_) {
                        // An empty "after" is how ComponentEditCommand expresses a removal.
                        DrawDiffed(editor, entity, placeholder.name, "Remove " + placeholder.name,
                                   [&] { placeholder.removeComponentFunc(entity, world); });
                        break;
                    }
                }
            }

            if (placement) {
                ImGui::Spacing();
                MutedText("The rest of this entity belongs to its prefab: open the prefab to change it, expose a "
                          "parameter for it, or unpack this placement.");
            } else {
                DrawAddComponent(editor, entity);
            }
            openRequest_.reset();
        }
        FlushEdits(editor);
    }
    EndWindow();
}

void InspectorEditor::DrawDiffed(EditorApplication& editor, Entity entity, const std::string& componentName,
                                 const std::string& label, const std::function<void()>& draw) {
    auto* world = editor.GetWorld();
    // A component with no XML tag has no saver, so it isn't written to the scene either and
    // there is nothing meaningful to restore. Draw it, don't record it.
    const std::string tag = ComponentRegistry::Instance().GetXmlTag(componentName);
    if (!world || tag.empty()) {
        draw();
        return;
    }

    std::string before = EntityXml::SaveComponent(*world, entity, tag);
    draw();
    std::string after = EntityXml::SaveComponent(*world, entity, tag);
    if (after == before) return;

    pendingEdits_.push_back({entity, tag, std::move(before), std::move(after), label});
}

void InspectorEditor::FlushEdits(EditorApplication& editor) {
    // IsAnyItemActive is global, so it is paired with a focus check: a drag in the Viewport
    // shouldn't hold this panel's gesture open and swallow a later, unrelated edit into it.
    const bool holding = ImGui::IsAnyItemActive() && ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);

    if (holding && !gestureOpen_) {
        editor.BeginGesture(pendingEdits_.empty() ? "Edit Component" : pendingEdits_.front().label);
        gestureOpen_ = true;
    }

    for (auto& edit : pendingEdits_) {
        editor.Execute(std::make_unique<ComponentEditCommand>(EntityRef{editor.StableIdOf(edit.entity)},
                                                               edit.component, edit.before, edit.after, edit.label));
    }
    pendingEdits_.clear();

    if (!holding && gestureOpen_) {
        editor.EndGesture();
        gestureOpen_ = false;
    }
}

void InspectorEditor::DrawHeader(EditorApplication& editor, Entity entity) {
    auto* world = editor.GetWorld();

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
    DrawDiffed(editor, entity, NameComponent::Name(), "Rename Entity", [&] {
        if (!ImGui::InputTextWithHint("##EntityName", "Unnamed entity", nameBuffer_, sizeof(nameBuffer_))) return;
        if (world->HasComponent<NameComponent>(entity)) {
            world->GetComponent<NameComponent>(entity).name = nameBuffer_;
        } else {
            world->AddComponent(entity, NameComponent(nameBuffer_));
        }
    });
    ImGui::SameLine();
    ImGui::TextDisabled("%s", id.c_str());
    ImGui::SameLine();
    ExpandCollapseButtons(openRequest_);
    ImGui::Spacing();
}

void InspectorEditor::DrawComponent(EditorApplication& editor, Entity entity, const ComponentPlaceholder& placeholder,
                                    bool removable) {
    auto* world = editor.GetWorld();
    ImGui::PushID(placeholder.name.c_str());

    ApplyOpenRequest(openRequest_);
    const bool open = CollapsingSection(placeholder.name.c_str(), true);

    // Per-component actions (not on a placed prefab, whose components are its prefab's).
    if (removable) {
        AlignRight(ButtonWidth(ICON_FA_ELLIPSIS_VERTICAL));
        if (IconButton(ICON_FA_ELLIPSIS_VERTICAL, "Component actions")) ImGui::OpenPopup("ComponentActions");
        if (ImGui::BeginPopup("ComponentActions")) {
            if (ImGui::MenuItem(ICON_FA_ROTATE_LEFT "  Reset")) {
                DrawDiffed(editor, entity, placeholder.name, "Reset " + placeholder.name,
                           [&] { placeholder.resetComponentFunc(entity, world); });
            }
            ImGui::PushStyleColor(ImGuiCol_Text, Palette().Error);
            if (ImGui::MenuItem(ICON_FA_TRASH_CAN "  Remove")) componentToRemove_ = placeholder.name;
            ImGui::PopStyleColor();
            ImGui::EndPopup();
        }
    }

    if (open) {
        BeginSectionBody();
        DrawDiffed(editor, entity, placeholder.name, "Edit " + placeholder.name,
                   [&] { placeholder.drawFunc(entity, world); });
        EndSectionBody();
    }
    ImGui::PopID();
}

void InspectorEditor::DrawAddComponent(EditorApplication& editor, Entity entity) {
    auto* world = editor.GetWorld();

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

    for (const auto& placeholder : editor.GetComponentPlaceholders()) {
        if (placeholder.hasComponentFunc(entity, world)) continue;
        if (!MatchesSearch(placeholder.name, componentSearch_)) continue;

        if (ImGui::Selectable(placeholder.name.c_str())) {
            DrawDiffed(editor, entity, placeholder.name, "Add " + placeholder.name,
                       [&] { placeholder.addComponentFunc(entity, world); });
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::EndPopup();
}

}  // namespace Elysium
