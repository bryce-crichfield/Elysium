#pragma once

#include "imgui.h"

#include "Core/Editor.h"

namespace Elysium {

// Every metric the editor lays out with: spacing, rounding, borders and the shared column
// widths that keep panels lined up. Colors live in Editor::Palette.
struct Editor::Theme {
    // Spacing.
    static constexpr ImVec2 WindowPadding    = ImVec2(10, 10);
    static constexpr ImVec2 FramePadding     = ImVec2(8, 5);
    static constexpr ImVec2 ItemSpacing      = ImVec2(8, 6);
    static constexpr ImVec2 ItemInnerSpacing = ImVec2(6, 4);
    static constexpr ImVec2 CellPadding      = ImVec2(8, 4);
    static constexpr float IndentSpacing     = 14.0f;
    static constexpr float ScrollbarSize     = 12.0f;
    static constexpr float GrabMinSize       = 10.0f;

    // Rounding.
    static constexpr float WindowRounding    = 6.0f;
    static constexpr float ChildRounding     = 6.0f;
    static constexpr float FrameRounding     = 4.0f;
    static constexpr float PopupRounding     = 6.0f;
    static constexpr float ScrollbarRounding = 6.0f;
    static constexpr float GrabRounding      = 4.0f;
    static constexpr float TabRounding       = 4.0f;

    // Border thickness.
    static constexpr float WindowBorder = 1.0f;
    static constexpr float ChildBorder  = 1.0f;
    static constexpr float PopupBorder  = 1.0f;
    static constexpr float FrameBorder  = 0.0f;

    // Shared layout.
    static constexpr float LabelColumnWidth = 130.0f;  // property label column in every inspector
    static constexpr float SearchWidth      = 220.0f;  // search box in a toolbar row
    static constexpr float IdColumnWidth    = 56.0f;
    static constexpr float DropZoneHeight   = 4.0f;    // hierarchy reorder target
    static constexpr float DropLineWidth    = 2.0f;
    static constexpr float DialogWidth      = 460.0f;  // modal dialogs opened from the menu bar

    // Viewport overlays.
    static constexpr float AxisWidth          = 1.5f;
    static constexpr float OverlayLineWidth   = 2.0f;
    static constexpr float MoveHandleRadius   = 8.0f;
    static constexpr float SelectionFallback  = 16.0f;  // half-size box for entities without bounds
    static constexpr float ClickCycleDistance = 4.0f;   // re-clicks this close cycle overlapping hits

    // Typography.
    static constexpr const char* FontFile = "Fonts/FiraCode-Regular.ttf";  // engine asset
    static constexpr float FontSize       = 15.0f;

    // Installs the palette and metrics above into ImGui's global style.
    static void Apply();
    // Adds the UI font (with the icon font merged in) and makes it the default. Call after
    // the ImGui backend is set up, and again whenever the font atlas is rebuilt.
    static void LoadFonts();
};

}  // namespace Elysium
