#include "Editor/LayerDrawer.h"

#include <algorithm>
#include <vector>

#include "Core/AssetKind.h"
#include "Core/Editor.h"
#include "Core/Scene.h"
#include "Editor/AssetField.h"
#include "Editor/AssetStyle.h"
#include "Editor/Theme.h"
#include "Editor/Widgets.h"
#include "Interfaces/IEditorService.h"
#include "imgui.h"

namespace Elysium {

using namespace Services;
using EditorStyle::Palette;

namespace {

constexpr float kDrawerWidth = 260.0f;

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

bool LayerDrawer::DrawToolbarButton() {
    if (!ToggleIconButton(ICON_FA_TABLE_CELLS, open_, "Layers")) return false;
    open_ = !open_;
    return true;
}

void LayerDrawer::Draw(Scene& scene, IEditorService& editor, Rectangle imageScreenRect) {
    if (!open_) return;

    auto& layers = scene.GetLayers();
    if (layers.empty()) return;

    // Top-down: highest z first, so the list reads the way the scene stacks on screen.
    std::vector<const SceneLayer*> ordered;
    ordered.reserve(layers.size());
    for (const auto& layer : layers) ordered.push_back(&layer);
    std::stable_sort(ordered.begin(), ordered.end(),
                     [](const SceneLayer* a, const SceneLayer* b) { return a->zIndex > b->zIndex; });

    const float width = std::min(kDrawerWidth, imageScreenRect.width);
    // A child window takes its position from the cursor, not SetNextWindowPos, so park the cursor
    // at the viewport image's right edge to anchor the drawer there.
    ImGui::SetCursorScreenPos(ImVec2(imageScreenRect.x + imageScreenRect.width - width, imageScreenRect.y));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Palette::WithAlpha(Editor::Palette().Base, 0.94f));
    if (ImGui::BeginChild("##LayerDrawer", ImVec2(width, imageScreenRect.height),
                          ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar)) {
        SectionHeader("Layers");

        const bool anyFilter = !editor.GetActiveLayer().empty();
        if (anyFilter) {
            MutedText("Filtering Hierarchy");
            ImGui::SameLine();
            if (IconButton(ICON_FA_XMARK "##clearlayer", "Clear filter")) editor.SetActiveLayer("");
        } else {
            MutedText("Click a layer to focus it");
        }
        ImGui::Separator();

        const float listHeight = ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() * 5.0f;
        if (ImGui::BeginChild("##LayerList", ImVec2(0, std::max(listHeight, ImGui::GetFrameHeight())))) {
            for (const SceneLayer* layer : ordered) DrawLayerRow(scene, editor, layer->name, layer->zIndex);
        }
        ImGui::EndChild();

        ImGui::Separator();
        DrawBrushSection(editor);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
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

void LayerDrawer::DrawBrushSection(IEditorService& editor) {
    SectionHeader("Paint");

    const std::string& active = editor.GetActiveLayer();
    AssetField("##brush", AssetKind::Prefab, brushPrefab_);

    const bool canPaint = !brushPrefab_.empty() && !active.empty() && !editor.IsLayerLocked(active);
    ImGui::BeginDisabled(!canPaint);
    if (ToggleIconButton(ICON_FA_PAINT_ROLLER, paintMode_, "Paint prefab (click to place, drag to fill)")) {
        paintMode_ = !paintMode_;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();

    // Say exactly what is missing rather than leaving a dead button.
    if (brushPrefab_.empty())                  MutedText("Pick a prefab to paint");
    else if (active.empty())                   MutedText("Focus a layer to paint onto");
    else if (editor.IsLayerLocked(active))     ColoredText(Editor::Palette().Warning, "Layer is locked");
    else if (paintMode_)                       ColoredText(Editor::Palette().Accent, ("Painting onto " + active).c_str());
    else                                       MutedText(("Paints onto " + active).c_str());

    if (!canPaint) paintMode_ = false;

    GridSettings& grid = editor.GetGrid();
    if (paintMode_ && !grid.snapEnabled) {
        MutedText("Snap is off — placing freehand");
    }
}

}  // namespace Elysium
