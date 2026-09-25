#pragma once

#include <cstdint>

#include "imgui.h"

#include "Core/Editor.h"

namespace Elysium {

namespace PaletteDetail {
// Free functions so Palette's constants can call them while the struct is still incomplete.
constexpr ImVec4 Hex(uint32_t rgb, float alpha = 1.0f) {
    return ImVec4(((rgb >> 16) & 0xFF) / 255.0f, ((rgb >> 8) & 0xFF) / 255.0f, (rgb & 0xFF) / 255.0f, alpha);
}
constexpr ImVec4 WithAlpha(ImVec4 color, float alpha) { return ImVec4(color.x, color.y, color.z, alpha); }
}  // namespace PaletteDetail

// Every color the editor draws with. Panels and inspectors name a role from here rather
// than writing an ImVec4/IM_COL32 literal; Theme::Apply maps these onto ImGui's style.
struct Editor::Palette {
    static constexpr ImVec4 WithAlpha(ImVec4 color, float alpha) { return PaletteDetail::WithAlpha(color, alpha); }
    static ImU32 ToU32(const ImVec4& color) { return ImGui::ColorConvertFloat4ToU32(color); }

    // Slate surfaces, darkest to lightest.
    static constexpr ImVec4 Crust    = PaletteDetail::Hex(0x131519);  // menu bar, title bars, empty dock space
    static constexpr ImVec4 Base     = PaletteDetail::Hex(0x1B1E23);  // window background
    static constexpr ImVec4 Mantle   = PaletteDetail::Hex(0x23272E);  // child regions, popups, table headers
    static constexpr ImVec4 Surface0 = PaletteDetail::Hex(0x2F353E);  // input frames, buttons
    static constexpr ImVec4 Surface1 = PaletteDetail::Hex(0x3C434E);  // hovered frames
    static constexpr ImVec4 Surface2 = PaletteDetail::Hex(0x4A5361);  // pressed frames
    static constexpr ImVec4 Border   = PaletteDetail::Hex(0x3A414B);

    // Text.
    static constexpr ImVec4 Text         = PaletteDetail::Hex(0xE8ECF1);
    static constexpr ImVec4 TextMuted    = PaletteDetail::Hex(0x9BA4B1);  // labels, secondary info
    static constexpr ImVec4 TextDisabled = PaletteDetail::Hex(0x68707C);

    // Accent: selection, focus, checkmarks, sliders.
    static constexpr ImVec4 Accent       = PaletteDetail::Hex(0x5B9BD5);
    static constexpr ImVec4 AccentHover  = PaletteDetail::Hex(0x74ADE0);
    static constexpr ImVec4 AccentActive = PaletteDetail::Hex(0x4A88C2);
    static constexpr ImVec4 AccentSoft   = PaletteDetail::WithAlpha(Accent, 0.28f);  // selected-row fill

    // Status.
    static constexpr ImVec4 Success = PaletteDetail::Hex(0x7CC68D);
    static constexpr ImVec4 Warning = PaletteDetail::Hex(0xE8C170);
    static constexpr ImVec4 Error   = PaletteDetail::Hex(0xE57A7A);
    static constexpr ImVec4 Debug   = PaletteDetail::Hex(0x86B3D1);

    // Viewport overlays.
    static constexpr ImVec4 AxisX         = PaletteDetail::Hex(0xE06C6C);
    static constexpr ImVec4 AxisY         = PaletteDetail::Hex(0x7CC68D);
    static constexpr ImVec4 CameraBounds  = PaletteDetail::WithAlpha(AxisX, 0.85f);
    static constexpr ImVec4 Selection     = Warning;
    static constexpr ImVec4 HandleFill    = Accent;
    static constexpr ImVec4 HandleOutline = Crust;
    static constexpr ImVec4 HandleGlyph   = Text;
};

}  // namespace Elysium
