#include "ScenesEditor.h"
#include <algorithm>
#include <vector>
#include "Core/Common.h"
#include "Core/Scene.h"
#include "Editor/Widgets.h"
#include "Interfaces/IEditorService.h"
#include "Interfaces/ISceneService.h"

namespace Elysium {

using namespace Services;

ScenesEditor::ScenesEditor(ServiceLocator& services) : Editor(services, Title) {}

void ScenesEditor::Draw() {
    Profile;

    auto& scenes = services_.Get<ISceneService>();
    auto& editor = services_.Get<IEditorService>();

    if (BeginWindow()) {
        DrawToolbar(scenes);
        DrawAvailable(scenes);
        DrawStack(scenes, editor);
    }
    EndWindow();
}

void ScenesEditor::DrawToolbar(ISceneService& scenes) {
    const bool hasSelection = scenes.GetSceneRegistry().count(selectedSceneName_) > 0;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float buttons = ButtonWidth(ICON_FA_PLUS) + ButtonWidth(ICON_FA_ARROW_RIGHT_ARROW_LEFT) +
                          ButtonWidth(ICON_FA_ARROW_UP_FROM_BRACKET) + ButtonWidth(ICON_FA_TRASH_CAN) + spacing * 4;

    SearchField("##search", search_, sizeof(search_), -buttons);

    ImGui::SameLine();
    ImGui::BeginDisabled(!hasSelection);
    if (IconButton(ICON_FA_PLUS, "Push the selected scene onto the stack")) scenes.Push(selectedSceneName_);
    ImGui::SameLine();
    if (IconButton(ICON_FA_ARROW_RIGHT_ARROW_LEFT, "Replace the top of the stack with the selected scene")) {
        scenes.Replace(selectedSceneName_);
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(scenes.IsEmpty());
    if (IconButton(ICON_FA_ARROW_UP_FROM_BRACKET, "Pop the top scene")) scenes.Pop();
    ImGui::SameLine();
    if (IconButton(ICON_FA_TRASH_CAN, "Clear the stack")) scenes.Clear();
    ImGui::EndDisabled();
}

void ScenesEditor::DrawAvailable(ISceneService& scenes) {
    SectionHeader("Available");

    // The registry is unordered; list it alphabetically so rows don't shuffle.
    std::vector<std::pair<const std::string*, const SceneRegistration*>> sorted;
    for (const auto& [name, registration] : scenes.GetSceneRegistry()) {
        if (MatchesSearch(name, search_)) sorted.emplace_back(&name, &registration);
    }
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return *a.first < *b.first; });

    if (sorted.empty()) {
        ImGui::TextDisabled(*search_ ? "No matching scenes" : "No scenes registered");
        return;
    }

    for (const auto& [name, registration] : sorted) {
        const bool loaded = scenes.IsInStack(registration->scene);
        const std::string tooltip = registration->xmlPath + (registration->xmlPath.empty() ? "" : "\n") + "Double-click to push";
        if (ListRow(name->c_str(), selectedSceneName_ == *name, loaded ? ICON_FA_CIRCLE_CHECK : ICON_FA_FILE,
                    loaded ? Palette().Success : Palette().TextMuted, name->c_str(), nullptr, tooltip.c_str())) {
            selectedSceneName_ = *name;
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) scenes.Push(*name);
        }
    }
}

void ScenesEditor::DrawStack(ISceneService& scenes, IEditorService& editor) {
    SectionHeader("Stack");

    const auto& stack = scenes.GetStack();
    if (stack.empty()) {
        ImGui::TextDisabled("Empty. Double-click a scene to push it.");
        return;
    }

    const Scene* inspected = editor.GetInspectedScene();
    for (int i = (int)stack.size() - 1; i >= 0; --i) {
        const bool isTop = i == (int)stack.size() - 1;
        const std::string name = scenes.GetSceneName(stack[i]);
        ImGui::PushID(i);
        if (ListRow(name.c_str(), stack[i] == inspected, isTop ? ICON_FA_LAYER_GROUP : ICON_FA_BARS_STAGGERED,
                    isTop ? Palette().Accent : Palette().TextMuted, name.c_str(), isTop ? "top" : nullptr,
                    "Inspect in the Scene panel")) {
            editor.SetInspectedScene(stack[i]);
        }
        ImGui::PopID();
    }
}

}  // namespace Elysium
