#include "Editor/Style/Theme.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <set>

#include <tinyxml2.h>

#include "Editor/Editor.h"
#include "Core/Log.h"
#include "Core/Path.h"

// rlImGui's icon loader: merges FontAwesome into the most recently added font.
void SetupFontAwesome(void);

namespace Elysium {

const EditorStyle::Palette& Editor::Palette() { return EditorStyle::CurrentPalette(); }
const EditorStyle::Theme& Editor::Theme() { return EditorStyle::CurrentTheme(); }

namespace EditorStyle {

namespace {
constexpr const char* ThemeDirectory = "Editor/Themes";

// Theme file names for each field, one table per value type.
template <typename Owner, typename T>
struct Field {
    const char* name;
    T Owner::*member;
};

#define FIELD(Owner, name) Field<Owner, decltype(Owner::name)>{#name, &Owner::name}
constexpr Field<Palette, ImVec4> ColorFields[] = {
    FIELD(Palette, Crust),        FIELD(Palette, Base),          FIELD(Palette, Mantle),
    FIELD(Palette, Surface0),     FIELD(Palette, Surface1),      FIELD(Palette, Surface2),
    FIELD(Palette, Border),       FIELD(Palette, Text),          FIELD(Palette, TextMuted),
    FIELD(Palette, TextDisabled), FIELD(Palette, TextOnAccent),  FIELD(Palette, Accent),
    FIELD(Palette, AccentHover),  FIELD(Palette, AccentActive),  FIELD(Palette, Success),
    FIELD(Palette, Warning),      FIELD(Palette, Error),         FIELD(Palette, Debug),
    FIELD(Palette, AxisX),        FIELD(Palette, AxisY),
    FIELD(Palette, AccentSoft),
    FIELD(Palette, CameraBounds), FIELD(Palette, Selection),     FIELD(Palette, ViewportBackground),
    FIELD(Palette, AssetFolder),  FIELD(Palette, AssetScene),    FIELD(Palette, AssetPrefab),
    FIELD(Palette, AssetScript),  FIELD(Palette, AssetSound),    FIELD(Palette, AssetSprite),
    FIELD(Palette, AssetTexture), FIELD(Palette, AssetShader),  FIELD(Palette, AssetModel), FIELD(Palette, AssetAnimation), FIELD(Palette, AssetFont),
};
constexpr Field<Theme, ImVec2> Vec2Fields[] = {
    FIELD(Theme, WindowPadding), FIELD(Theme, FramePadding), FIELD(Theme, ItemSpacing),
    FIELD(Theme, ItemInnerSpacing), FIELD(Theme, CellPadding),
};
constexpr Field<Theme, float> FloatFields[] = {
    FIELD(Theme, IndentSpacing),     FIELD(Theme, ScrollbarSize),      FIELD(Theme, GrabMinSize),
    FIELD(Theme, WindowRounding),    FIELD(Theme, ChildRounding),      FIELD(Theme, FrameRounding),
    FIELD(Theme, PopupRounding),     FIELD(Theme, ScrollbarRounding),  FIELD(Theme, GrabRounding),
    FIELD(Theme, TabRounding),       FIELD(Theme, WindowBorder),       FIELD(Theme, ChildBorder),
    FIELD(Theme, PopupBorder),       FIELD(Theme, FrameBorder),        FIELD(Theme, LabelColumnWidth),
    FIELD(Theme, SearchWidth),       FIELD(Theme, IdColumnWidth),      FIELD(Theme, DropZoneHeight),
    FIELD(Theme, DropLineWidth),     FIELD(Theme, DialogWidth),        FIELD(Theme, AxisWidth),
    FIELD(Theme, OverlayLineWidth),  FIELD(Theme, SelectionFallback),
    FIELD(Theme, ClickCycleDistance), FIELD(Theme, FontSize),
};
#undef FIELD

template <typename Owner, typename T, size_t N>
T* FindField(Owner& owner, const Field<Owner, T> (&fields)[N], const char* name) {
    for (const auto& field : fields) {
        if (std::strcmp(field.name, name) == 0) return &(owner.*field.member);
    }
    return nullptr;
}

// "#RRGGBB" or "#RRGGBBAA".
bool ParseColor(const char* text, ImVec4& out) {
    if (!text || *text != '#') return false;
    const size_t length = std::strlen(text + 1);
    if (length != 6 && length != 8) return false;
    char* end = nullptr;
    const uint32_t value = (uint32_t)std::strtoul(text + 1, &end, 16);
    if (*end) return false;
    out = length == 6 ? Hex(value) : Hex(value >> 8, (value & 0xFF) / 255.0f);
    return true;
}

// Fills the derived colors the theme file didn't set itself.
void Derive(Palette& p, const std::set<std::string>& given) {
    auto derive = [&](const char* name, ImVec4& color, ImVec4 value) {
        if (!given.count(name)) color = value;
    };
    derive("AccentSoft", p.AccentSoft, Palette::WithAlpha(p.Accent, 0.28f));
    derive("CameraBounds", p.CameraBounds, Palette::WithAlpha(p.AxisX, 0.85f));
    derive("Selection", p.Selection, p.Warning);
}

// Applies a theme file over `t` and `p`, recording which colors it set. Bad entries are
// logged and skipped.
bool ReadThemeFile(const std::string& path, Theme& t, Palette& p, std::set<std::string>& givenColors) {
    tinyxml2::XMLDocument document;
    if (document.LoadFile(path.c_str()) != tinyxml2::XML_SUCCESS) {
        LOG_ERROR("Theme", "Failed to load theme file: " + path);
        return false;
    }
    const tinyxml2::XMLElement* root = document.FirstChildElement("Theme");
    if (!root) {
        LOG_ERROR("Theme", "Missing <Theme> root in " + path);
        return false;
    }

    if (const auto* colors = root->FirstChildElement("Palette")) {
        for (const auto* e = colors->FirstChildElement("Color"); e; e = e->NextSiblingElement("Color")) {
            const char* name = e->Attribute("name");
            ImVec4* color = name ? FindField(p, ColorFields, name) : nullptr;
            if (!color || !ParseColor(e->Attribute("value"), *color)) {
                LOG_WARNING("Theme", std::string("Skipping bad color '") + (name ? name : "") + "' in " + path);
                continue;
            }
            givenColors.insert(name);
        }
    }

    if (const auto* metrics = root->FirstChildElement("Metrics")) {
        for (const auto* e = metrics->FirstChildElement("Metric"); e; e = e->NextSiblingElement("Metric")) {
            const char* name = e->Attribute("name");
            if (!name) continue;
            if (float* value = FindField(t, FloatFields, name)) {
                e->QueryFloatAttribute("value", value);
            } else if (ImVec2* value = FindField(t, Vec2Fields, name)) {
                e->QueryFloatAttribute("x", &value->x);
                e->QueryFloatAttribute("y", &value->y);
            } else {
                LOG_WARNING("Theme", std::string("Skipping unknown metric '") + name + "' in " + path);
            }
        }
    }

    if (const auto* font = root->FirstChildElement("Font")) {
        if (const char* file = font->Attribute("file")) t.FontFile = file;
        font->QueryFloatAttribute("size", &t.FontSize);
    }
    if (const auto* font = root->FirstChildElement("EditorFont")) {
        if (const char* file = font->Attribute("file")) t.EditorFont = file;
    }
    return true;
}

// Installs the theme and palette into ImGui's global style.
void Install(const Theme& t, const Palette& P) {
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowPadding     = t.WindowPadding;
    style.FramePadding      = t.FramePadding;
    style.ItemSpacing       = t.ItemSpacing;
    style.ItemInnerSpacing  = t.ItemInnerSpacing;
    style.CellPadding       = t.CellPadding;
    style.IndentSpacing     = t.IndentSpacing;
    style.ScrollbarSize     = t.ScrollbarSize;
    style.GrabMinSize       = t.GrabMinSize;

    style.WindowRounding    = t.WindowRounding;
    style.ChildRounding     = t.ChildRounding;
    style.FrameRounding     = t.FrameRounding;
    style.PopupRounding     = t.PopupRounding;
    style.ScrollbarRounding = t.ScrollbarRounding;
    style.GrabRounding      = t.GrabRounding;
    style.TabRounding       = t.TabRounding;

    style.WindowBorderSize  = t.WindowBorder;
    style.ChildBorderSize   = t.ChildBorder;
    style.PopupBorderSize   = t.PopupBorder;
    style.FrameBorderSize   = t.FrameBorder;
    style.TabBorderSize     = 0.0f;
    style.TabBarBorderSize  = 1.0f;

    style.WindowTitleAlign         = ImVec2(0.0f, 0.5f);
    style.WindowMenuButtonPosition = ImGuiDir_None;
    style.SeparatorTextBorderSize  = 1.0f;
    style.SeparatorTextPadding     = ImVec2(0.0f, 4.0f);

    ImVec4* c = style.Colors;
    c[ImGuiCol_Text]                  = P.Text;
    c[ImGuiCol_TextDisabled]          = P.TextDisabled;
    c[ImGuiCol_WindowBg]              = P.Base;
    c[ImGuiCol_ChildBg]               = Palette::WithAlpha(P.Base, 0.0f);
    c[ImGuiCol_PopupBg]               = P.Mantle;
    c[ImGuiCol_Border]                = P.Border;
    c[ImGuiCol_BorderShadow]          = Palette::WithAlpha(P.Crust, 0.0f);
    c[ImGuiCol_FrameBg]               = P.Surface0;
    c[ImGuiCol_FrameBgHovered]        = P.Surface1;
    c[ImGuiCol_FrameBgActive]         = P.Surface2;
    c[ImGuiCol_TitleBg]               = P.Crust;
    c[ImGuiCol_TitleBgActive]         = P.Crust;
    c[ImGuiCol_TitleBgCollapsed]      = P.Crust;
    c[ImGuiCol_MenuBarBg]             = P.Crust;
    c[ImGuiCol_ScrollbarBg]           = Palette::WithAlpha(P.Base, 0.0f);
    c[ImGuiCol_ScrollbarGrab]         = P.Surface1;
    c[ImGuiCol_ScrollbarGrabHovered]  = P.Surface2;
    c[ImGuiCol_ScrollbarGrabActive]   = P.TextDisabled;
    c[ImGuiCol_CheckMark]             = P.Accent;
    c[ImGuiCol_SliderGrab]            = P.Accent;
    c[ImGuiCol_SliderGrabActive]      = P.AccentActive;
    c[ImGuiCol_Button]                = P.Surface0;
    c[ImGuiCol_ButtonHovered]         = P.Surface1;
    c[ImGuiCol_ButtonActive]          = P.Surface2;
    c[ImGuiCol_Header]                = P.AccentSoft;
    c[ImGuiCol_HeaderHovered]         = P.Surface1;
    c[ImGuiCol_HeaderActive]          = P.Surface2;
    c[ImGuiCol_Separator]             = P.Border;
    c[ImGuiCol_SeparatorHovered]      = P.Accent;
    c[ImGuiCol_SeparatorActive]       = P.AccentActive;
    c[ImGuiCol_ResizeGrip]            = Palette::WithAlpha(P.Surface1, 0.0f);
    c[ImGuiCol_ResizeGripHovered]     = P.AccentSoft;
    c[ImGuiCol_ResizeGripActive]      = P.Accent;
    c[ImGuiCol_InputTextCursor]       = P.Text;
    c[ImGuiCol_Tab]                   = P.Crust;
    c[ImGuiCol_TabHovered]            = P.Surface1;
    c[ImGuiCol_TabSelected]           = P.Base;
    c[ImGuiCol_TabSelectedOverline]   = P.Accent;
    c[ImGuiCol_TabDimmed]             = P.Crust;
    c[ImGuiCol_TabDimmedSelected]     = P.Base;
    c[ImGuiCol_TabDimmedSelectedOverline] = Palette::WithAlpha(P.Accent, 0.0f);
    c[ImGuiCol_DockingPreview]        = P.AccentSoft;
    c[ImGuiCol_DockingEmptyBg]        = P.Crust;
    c[ImGuiCol_PlotLines]             = P.Accent;
    c[ImGuiCol_PlotLinesHovered]      = P.AccentHover;
    c[ImGuiCol_PlotHistogram]         = P.Accent;
    c[ImGuiCol_PlotHistogramHovered]  = P.AccentHover;
    c[ImGuiCol_TableHeaderBg]         = P.Mantle;
    c[ImGuiCol_TableBorderStrong]     = P.Border;
    c[ImGuiCol_TableBorderLight]      = Palette::WithAlpha(P.Border, 0.6f);
    c[ImGuiCol_TableRowBg]            = Palette::WithAlpha(P.Base, 0.0f);
    c[ImGuiCol_TableRowBgAlt]         = Palette::WithAlpha(P.Surface0, 0.35f);
    c[ImGuiCol_TextLink]              = P.Accent;
    c[ImGuiCol_TextSelectedBg]        = P.AccentSoft;
    c[ImGuiCol_TreeLines]             = P.Border;
    c[ImGuiCol_DragDropTarget]        = P.Accent;
    c[ImGuiCol_NavCursor]             = P.Accent;
    c[ImGuiCol_NavWindowingHighlight] = Palette::WithAlpha(P.Text, 0.7f);
    c[ImGuiCol_NavWindowingDimBg]     = Palette::WithAlpha(P.Crust, 0.6f);
    c[ImGuiCol_ModalWindowDimBg]      = Palette::WithAlpha(P.Crust, 0.6f);
}


Theme theme;
Palette palette;
}  // namespace

const Theme& CurrentTheme() { return theme; }
const Palette& CurrentPalette() { return palette; }

std::vector<std::string> AvailableThemes() {
    std::vector<std::string> names;
    std::error_code error;
    const std::filesystem::path directory = Path(ThemeDirectory, PathRoot::Engine).GetFullPath();
    for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
        if (entry.path().extension() == ".xml") names.push_back(entry.path().stem().string());
    }
    std::sort(names.begin(), names.end());
    return names;
}

bool LoadTheme(const std::string& name) {
    Theme t;
    Palette p;
    std::set<std::string> givenColors;
    const std::string path = Path(std::string(ThemeDirectory) + "/" + name + ".xml", PathRoot::Engine).GetFullPath();
    if (ReadThemeFile(path, t, p, givenColors)) {
        t.name = name;
    } else {
        t = Theme{};
        p = Palette{};
        givenColors.clear();
    }
    Derive(p, givenColors);

    const bool fontChanged = t.FontFile != theme.FontFile || t.FontSize != theme.FontSize ||
                             t.EditorFont != theme.EditorFont;
    theme = std::move(t);
    palette = p;
    Install(theme, palette);
    return fontChanged;
}

namespace {
ImFont* codeFont = nullptr;
}

void LoadFonts() {
    ImGuiIO& io = ImGui::GetIO();
    const std::string path = Path(theme.FontFile, PathRoot::Engine).GetFullPath();
    if (ImFont* font = io.Fonts->AddFontFromFileTTF(path.c_str(), theme.FontSize)) {
        SetupFontAwesome();
        io.FontDefault = font;
    }
    codeFont = io.Fonts->AddFontFromFileTTF(Path(theme.EditorFont, PathRoot::Engine).GetFullPath().c_str(), theme.FontSize);
}

ImFont* CodeFont() {
    return codeFont ? codeFont : ImGui::GetIO().FontDefault;
}

}  // namespace EditorStyle
}  // namespace Elysium
