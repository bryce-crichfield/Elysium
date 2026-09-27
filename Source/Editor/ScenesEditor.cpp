#include "ScenesEditor.h"
#include <algorithm>
#include <vector>
#include "Core/Common.h"
#include "Core/Path.h"
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
        DrawToolbar(scenes, editor);
        DrawAvailable(scenes, editor);
    }
    EndWindow();
}

void ScenesEditor::DrawToolbar(ISceneService& scenes, IEditorService& editor) {
    const bool hasSelection = scenes.GetSceneRegistry().count(selectedSceneName_) > 0;

    SearchField("##search", search_, sizeof(search_), -(ButtonWidth(ICON_FA_FOLDER_OPEN) + ImGui::GetStyle().ItemSpacing.x));
    ImGui::SameLine();
    ImGui::BeginDisabled(!hasSelection);
    if (IconButton(ICON_FA_FOLDER_OPEN, "Open the selected scene in a viewport tab")) editor.OpenScene(selectedSceneName_);
    ImGui::EndDisabled();
}

void ScenesEditor::DrawAvailable(ISceneService& scenes, IEditorService& editor) {
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
        // Open = has a tab in the viewport (the editor's own copy, not the game's stack).
        bool open = false;
        for (const auto& doc : editor.GetDocuments()) {
            if (!doc->IsPrefab() && SamePath(doc->fullPath, registration->xmlPath)) open = true;
        }
        const bool isEntry = *name == scenes.GetEntryScene();
        const std::string tooltip = registration->xmlPath + (registration->xmlPath.empty() ? "" : "\n") + "Double-click to open";
        if (ListRow(name->c_str(), selectedSceneName_ == *name, open ? ICON_FA_CIRCLE_CHECK : ICON_FA_FILE,
                    open ? Palette().Success : Palette().TextMuted, name->c_str(), isEntry ? "entry" : nullptr, tooltip.c_str())) {
            selectedSceneName_ = *name;
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) editor.OpenScene(*name);
        }
    }
}

}  // namespace Elysium
