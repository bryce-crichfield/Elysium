#pragma once

#include <cstdint>

#include "imgui.h"

namespace Elysium::EditorStyle {

constexpr ImVec4 Hex(uint32_t rgb, float alpha = 1.0f) {
    return ImVec4(((rgb >> 16) & 0xFF) / 255.0f, ((rgb >> 8) & 0xFF) / 255.0f, (rgb & 0xFF) / 255.0f, alpha);
}

// Every color the editor draws with. Panels and inspectors name a role from here rather
// than writing an ImVec4/IM_COL32 literal; ApplyTheme maps these onto ImGui's style.
// Defaults are the Dark theme; a theme file (Assets/Editor/Themes/*.xml) overrides any.
struct Palette {
    static constexpr ImVec4 WithAlpha(ImVec4 color, float alpha) { return ImVec4(color.x, color.y, color.z, alpha); }
    static ImU32 ToU32(const ImVec4& color) { return ImGui::ColorConvertFloat4ToU32(color); }

    // Surfaces, darkest to lightest in the dark theme.
    ImVec4 Crust    = Hex(0x131519);  // menu bar, title bars, empty dock space
    ImVec4 Base     = Hex(0x1B1E23);  // window background
    ImVec4 Mantle   = Hex(0x23272E);  // child regions, popups, table headers
    ImVec4 ViewportBackground = Hex(0x000000);  // empty world behind the scene in the editor viewport
    ImVec4 Surface0 = Hex(0x2F353E);  // input frames, buttons
    ImVec4 Surface1 = Hex(0x3C434E);  // hovered frames
    ImVec4 Surface2 = Hex(0x4A5361);  // pressed frames
    ImVec4 Border   = Hex(0x3A414B);

    // Text.
    ImVec4 Text         = Hex(0xE8ECF1);
    ImVec4 TextMuted    = Hex(0x9BA4B1);  // labels, secondary info
    ImVec4 TextDisabled = Hex(0x68707C);
    ImVec4 TextOnAccent = Hex(0x131519);  // primary button labels

    // Accent: selection, focus, checkmarks, sliders.
    ImVec4 Accent       = Hex(0x5B9BD5);
    ImVec4 AccentHover  = Hex(0x74ADE0);
    ImVec4 AccentActive = Hex(0x4A88C2);

    // Status.
    ImVec4 Success = Hex(0x7CC68D);
    ImVec4 Warning = Hex(0xE8C170);
    ImVec4 Error   = Hex(0xE57A7A);
    ImVec4 Debug   = Hex(0x86B3D1);

    // Viewport overlays.
    ImVec4 AxisX = Hex(0xE06C6C);
    ImVec4 AxisY = Hex(0x7CC68D);


    // Asset types: the color every view of an asset marks it with.
    ImVec4 AssetFolder  = Hex(0x8C9AAE);
    ImVec4 AssetScene   = Hex(0x5B9BD5);
    ImVec4 AssetPrefab  = Hex(0x6CBF7F);
    ImVec4 AssetScript  = Hex(0xE3C35A);
    ImVec4 AssetSound   = Hex(0xE06C6C);
    ImVec4 AssetSprite  = Hex(0xA98BE0);
    ImVec4 AssetTexture = Hex(0xE8995A);
    ImVec4 AssetShader  = Hex(0x5CC8D0);
    ImVec4 AssetModel   = Hex(0xE05CC8);
    ImVec4 AssetAnimation = Hex(0x7F63C9);  // a deeper Sprite

    // Derived from the colors above after a theme loads, unless the theme sets them.
    ImVec4 AccentSoft;     // selected-row fill
    ImVec4 CameraBounds;
    ImVec4 Selection;
};

}  // namespace Elysium::EditorStyle
