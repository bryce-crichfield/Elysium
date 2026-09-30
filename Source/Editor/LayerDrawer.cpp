#include "Editor/LayerDrawer.h"

#include <algorithm>
#include <vector>

#include "Core/Editor.h"
#include "Core/Scene.h"
#include "Editor/Theme.h"
#include "Editor/Widgets.h"
#include "Interfaces/IEditorService.h"
#include "imgui.h"

namespace Elysium {

using namespace Services;
using EditorStyle::Palette;

namespace {

// A small square toggle that keeps its slot whatever the state, so the row's columns line up.
bool RowToggle(const char* id, const char* onIcon, const char* offIcon, bool& value,
               const ImVec4& onColor, const char* tooltip) {
    const char* icon = value ? onIcon : offIcon;
    ImGui::PushStyleColor(ImGuiCol_Text, value ? onColor : Editor::Palette().TextDisabled);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));  // icon only; the row draws the fill
    const bool pressed = ImGui::Button((std::string(icon) + "##" + id).c_str(),
                                       ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()));
    ImGui::PopStyleColor(2);
    ItemTooltip(tooltip);
    if (pressed) value = !value;
    return pressed;
}

}  // namespace

void LayerDrawer::Draw(Scene& scene, IEditorService& editor, Rectangle imageScreenRect) {
    panel_.Draw(imageScreenRect, "Layers", [&] {
        // Top-down: highest z first, so the list reads the way the scene stacks on screen.
        std::vector<const SceneLayer*> ordered;
        auto& layers = scene.GetLayers();
        ordered.reserve(layers.size());
        for (const auto& layer : layers) ordered.push_back(&layer);
        std::stable_sort(ordered.begin(), ordered.end(),
                         [](const SceneLayer* a, const SceneLayer* b) { return a->zIndex > b->zIndex; });

        // A scene with no layers used to close the drawer out from under you; it now says so,
        // like every other empty panel in the editor.
        if (ordered.empty()) {
            EmptyState("This scene has no layers. Add one on the Settings screen.");
            return;
        }

        const bool anyFilter = !editor.GetActiveLayer().empty();
        if (anyFilter) {
            MutedText("Filtering Hierarchy");
            ImGui::SameLine();
            if (IconButton(ICON_FA_XMARK "##clearlayer", "Clear filter")) editor.SetActiveLayer("");
        } else {
            MutedText("Click a layer to focus it");
        }
        ImGui::Separator();

        if (ImGui::BeginChild("##LayerList", ImVec2(0, 0))) {
            for (const SceneLayer* layer : ordered) DrawLayerRow(scene, editor, layer->name, layer->zIndex);
        }
        ImGui::EndChild();
    });
}

void LayerDrawer::DrawLayerRow(Scene& scene, IEditorService& editor, const std::string& name, int zIndex) {
    ImGui::PushID(name.c_str());

    LayerEditState& state = editor.GetLayerState(name);
    const bool isActive = editor.GetActiveLayer() == name;
    // Effective, not the raw flag: a layer hidden only because something else is soloed should
    // still read as not drawing.
    const bool effectivelyHidden = editor.IsLayerHidden(name);

    const float toggles = ImGui::GetFrameHeight() * 3.0f + ImGui::GetStyle().ItemSpacing.x * 2.0f;
    const ImVec4& labelColor = effectivelyHidden ? Editor::Palette().TextDisabled
                             : state.locked      ? Editor::Palette().TextMuted
                                                 : Editor::Palette().Text;

    if (ImGui::Selectable("##row", isActive, ImGuiSelectableFlags_AllowOverlap,
                          ImVec2(0, ImGui::GetFrameHeight()))) {
        // Clicking the active layer again clears the filter, so focus is a toggle.
        editor.SetActiveLayer(isActive ? "" : name);
    }
    ImGui::SameLine(0.0f, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, labelColor);
    ImGui::Text("%s  %s", ICON_FA_LAYER_GROUP, name.c_str());
    ImGui::PopStyleColor();
    ItemTooltip(("z " + std::to_string(zIndex)).c_str());

    ImGui::SameLine();
    AlignRight(toggles);
    RowToggle("lock", ICON_FA_LOCK, ICON_FA_LOCK_OPEN, state.locked, Editor::Palette().Warning,
              state.locked ? "Unlock layer" : "Lock layer (no edits)");
    ImGui::SameLine();
    RowToggle("solo", ICON_FA_CIRCLE_DOT, ICON_FA_CIRCLE_DOT, state.solo, Editor::Palette().Accent,
              state.solo ? "Unsolo" : "Solo (hide all others)");
    ImGui::SameLine();
    RowToggle("hide", ICON_FA_EYE_SLASH, ICON_FA_EYE, state.hidden, Editor::Palette().Error,
              state.hidden ? "Show layer" : "Hide layer");

    ImGui::PopID();
}

}  // namespace Elysium
