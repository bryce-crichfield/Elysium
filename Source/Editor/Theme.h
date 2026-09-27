#pragma once

#include <string>
#include <vector>

#include "imgui.h"

#include "Editor/Palette.h"

namespace Elysium::EditorStyle {

// Every metric the editor lays out with: spacing, rounding, borders, the shared column
// widths that keep panels lined up, and the font. Colors live in the Palette. Defaults are
// the Dark theme; a theme file overrides any.
struct Theme {
    std::string name = "Dark";

    // Spacing.
    ImVec2 WindowPadding    = ImVec2(10, 10);
    ImVec2 FramePadding     = ImVec2(8, 5);
    ImVec2 ItemSpacing      = ImVec2(8, 6);
    ImVec2 ItemInnerSpacing = ImVec2(6, 4);
    ImVec2 CellPadding      = ImVec2(8, 4);
    float IndentSpacing     = 14.0f;
    float ScrollbarSize     = 12.0f;
    float GrabMinSize       = 10.0f;

    // Rounding.
    float WindowRounding    = 6.0f;
    float ChildRounding     = 6.0f;
    float FrameRounding     = 4.0f;
    float PopupRounding     = 6.0f;
    float ScrollbarRounding = 6.0f;
    float GrabRounding      = 4.0f;
    float TabRounding       = 4.0f;

    // Border thickness.
    float WindowBorder = 1.0f;
    float ChildBorder  = 1.0f;
    float PopupBorder  = 1.0f;
    float FrameBorder  = 0.0f;

    // Shared layout.
    float LabelColumnWidth = 130.0f;  // property label column in every inspector
    float SearchWidth      = 220.0f;  // search box in a toolbar row
    float IdColumnWidth    = 56.0f;
    float DropZoneHeight   = 4.0f;    // hierarchy reorder target
    float DropLineWidth    = 2.0f;
    float DialogWidth      = 460.0f;  // modal dialogs opened from the menu bar

    // Viewport overlays.
    float AxisWidth          = 1.5f;
    float OverlayLineWidth   = 2.0f;
    float SelectionFallback  = 16.0f;  // half-size box for entities without bounds
    float ClickCycleDistance = 4.0f;   // re-clicks this close cycle overlapping hits

    // Typography.
    std::string FontFile = "Fonts/FiraCode-Regular.ttf";  // engine asset
    float FontSize       = 15.0f;
    std::string EditorFont = "Fonts/FiraCode-Regular.ttf";  // script text editor
};

// The active theme and palette every editor panel reads.
const Theme& CurrentTheme();
const Palette& CurrentPalette();

// Theme files in Assets/Editor/Themes, by name (the file name without .xml).
std::vector<std::string> AvailableThemes();

// Resets to the defaults, applies Themes/<name>.xml over them and installs the result into
// ImGui's style. An unknown or broken file leaves the defaults. Returns true when the font
// changed, so the caller must rebuild the font atlas and call LoadFonts.
bool LoadTheme(const std::string& name);

// Adds the theme's font (with the icon font merged in) and makes it the default. Call after
// the ImGui backend is set up, and again whenever the font atlas is rebuilt.
void LoadFonts();

}  // namespace Elysium::EditorStyle
