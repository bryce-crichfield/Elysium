#include "Editor/Theme.h"

#include "Core/Path.h"
#include "Editor/Palette.h"

// rlImGui's icon loader: merges FontAwesome into the most recently added font.
void SetupFontAwesome(void);

namespace Elysium {

void Editor::Theme::LoadFonts() {
    ImGuiIO& io = ImGui::GetIO();
    const std::string path = Path(FontFile, PathRoot::Engine).GetFullPath();
    if (ImFont* font = io.Fonts->AddFontFromFileTTF(path.c_str(), FontSize)) {
        SetupFontAwesome();
        io.FontDefault = font;
    }
}

void Editor::Theme::Apply() {
    using P = Palette;
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowPadding     = WindowPadding;
    style.FramePadding      = FramePadding;
    style.ItemSpacing       = ItemSpacing;
    style.ItemInnerSpacing  = ItemInnerSpacing;
    style.CellPadding       = CellPadding;
    style.IndentSpacing     = IndentSpacing;
    style.ScrollbarSize     = ScrollbarSize;
    style.GrabMinSize       = GrabMinSize;

    style.WindowRounding    = WindowRounding;
    style.ChildRounding     = ChildRounding;
    style.FrameRounding     = FrameRounding;
    style.PopupRounding     = PopupRounding;
    style.ScrollbarRounding = ScrollbarRounding;
    style.GrabRounding      = GrabRounding;
    style.TabRounding       = TabRounding;

    style.WindowBorderSize  = WindowBorder;
    style.ChildBorderSize   = ChildBorder;
    style.PopupBorderSize   = PopupBorder;
    style.FrameBorderSize   = FrameBorder;
    style.TabBorderSize     = 0.0f;
    style.TabBarBorderSize  = 1.0f;

    style.WindowTitleAlign         = ImVec2(0.0f, 0.5f);
    style.WindowMenuButtonPosition = ImGuiDir_None;
    style.SeparatorTextBorderSize  = 1.0f;
    style.SeparatorTextPadding     = ImVec2(0.0f, 4.0f);

    ImVec4* c = style.Colors;
    c[ImGuiCol_Text]                  = P::Text;
    c[ImGuiCol_TextDisabled]          = P::TextDisabled;
    c[ImGuiCol_WindowBg]              = P::Base;
    c[ImGuiCol_ChildBg]               = P::WithAlpha(P::Base, 0.0f);
    c[ImGuiCol_PopupBg]               = P::Mantle;
    c[ImGuiCol_Border]                = P::Border;
    c[ImGuiCol_BorderShadow]          = P::WithAlpha(P::Crust, 0.0f);
    c[ImGuiCol_FrameBg]               = P::Surface0;
    c[ImGuiCol_FrameBgHovered]        = P::Surface1;
    c[ImGuiCol_FrameBgActive]         = P::Surface2;
    c[ImGuiCol_TitleBg]               = P::Crust;
    c[ImGuiCol_TitleBgActive]         = P::Crust;
    c[ImGuiCol_TitleBgCollapsed]      = P::Crust;
    c[ImGuiCol_MenuBarBg]             = P::Crust;
    c[ImGuiCol_ScrollbarBg]           = P::WithAlpha(P::Base, 0.0f);
    c[ImGuiCol_ScrollbarGrab]         = P::Surface1;
    c[ImGuiCol_ScrollbarGrabHovered]  = P::Surface2;
    c[ImGuiCol_ScrollbarGrabActive]   = P::TextDisabled;
    c[ImGuiCol_CheckMark]             = P::Accent;
    c[ImGuiCol_SliderGrab]            = P::Accent;
    c[ImGuiCol_SliderGrabActive]      = P::AccentActive;
    c[ImGuiCol_Button]                = P::Surface0;
    c[ImGuiCol_ButtonHovered]         = P::Surface1;
    c[ImGuiCol_ButtonActive]          = P::Surface2;
    c[ImGuiCol_Header]                = P::AccentSoft;
    c[ImGuiCol_HeaderHovered]         = P::Surface1;
    c[ImGuiCol_HeaderActive]          = P::Surface2;
    c[ImGuiCol_Separator]             = P::Border;
    c[ImGuiCol_SeparatorHovered]      = P::Accent;
    c[ImGuiCol_SeparatorActive]       = P::AccentActive;
    c[ImGuiCol_ResizeGrip]            = P::WithAlpha(P::Surface1, 0.0f);
    c[ImGuiCol_ResizeGripHovered]     = P::AccentSoft;
    c[ImGuiCol_ResizeGripActive]      = P::Accent;
    c[ImGuiCol_InputTextCursor]       = P::Text;
    c[ImGuiCol_Tab]                   = P::Crust;
    c[ImGuiCol_TabHovered]            = P::Surface1;
    c[ImGuiCol_TabSelected]           = P::Base;
    c[ImGuiCol_TabSelectedOverline]   = P::Accent;
    c[ImGuiCol_TabDimmed]             = P::Crust;
    c[ImGuiCol_TabDimmedSelected]     = P::Base;
    c[ImGuiCol_TabDimmedSelectedOverline] = P::WithAlpha(P::Accent, 0.0f);
    c[ImGuiCol_DockingPreview]        = P::AccentSoft;
    c[ImGuiCol_DockingEmptyBg]        = P::Crust;
    c[ImGuiCol_PlotLines]             = P::Accent;
    c[ImGuiCol_PlotLinesHovered]      = P::AccentHover;
    c[ImGuiCol_PlotHistogram]         = P::Accent;
    c[ImGuiCol_PlotHistogramHovered]  = P::AccentHover;
    c[ImGuiCol_TableHeaderBg]         = P::Mantle;
    c[ImGuiCol_TableBorderStrong]     = P::Border;
    c[ImGuiCol_TableBorderLight]      = P::WithAlpha(P::Border, 0.6f);
    c[ImGuiCol_TableRowBg]            = P::WithAlpha(P::Base, 0.0f);
    c[ImGuiCol_TableRowBgAlt]         = P::WithAlpha(P::Surface0, 0.35f);
    c[ImGuiCol_TextLink]              = P::Accent;
    c[ImGuiCol_TextSelectedBg]        = P::AccentSoft;
    c[ImGuiCol_TreeLines]             = P::Border;
    c[ImGuiCol_DragDropTarget]        = P::Accent;
    c[ImGuiCol_NavCursor]             = P::Accent;
    c[ImGuiCol_NavWindowingHighlight] = P::WithAlpha(P::Text, 0.7f);
    c[ImGuiCol_NavWindowingDimBg]     = P::WithAlpha(P::Crust, 0.6f);
    c[ImGuiCol_ModalWindowDimBg]      = P::WithAlpha(P::Crust, 0.6f);
}

}  // namespace Elysium
