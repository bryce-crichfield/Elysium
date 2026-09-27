#pragma once

#include "Core/AssetKind.h"
#include "Editor/Widgets.h"

namespace Elysium {

// The asset design language: every place the editor shows an asset marks it with its
// kind's color and icon.
struct AssetStyle {
    const char* label;
    const char* icon;
    ImVec4 color;
};

inline AssetStyle StyleOf(AssetKind kind) {
    const auto& palette = Editor::Palette();
    switch (kind) {
        case AssetKind::Folder: return {"Folder", ICON_FA_FOLDER, palette.AssetFolder};
        case AssetKind::Scene: return {"Scene", ICON_FA_CIRCLE_NODES, palette.AssetScene};
        case AssetKind::Prefab: return {"Prefab", ICON_FA_BOX, palette.AssetPrefab};
        case AssetKind::Script: return {"Script", ICON_FA_FILE_LINES, palette.AssetScript};
        case AssetKind::Sound: return {"Sound", ICON_FA_VOLUME_HIGH, palette.AssetSound};
        case AssetKind::Sprite: return {"Sprite", ICON_FA_PERSON_RUNNING, palette.AssetSprite};
        case AssetKind::Texture: return {"Texture", ICON_FA_IMAGE, palette.AssetTexture};
        case AssetKind::Shader: return {"Shader", ICON_FA_PAINTBRUSH, palette.AssetShader};
    }
    return {"Folder", ICON_FA_FOLDER, palette.AssetFolder};
}

// A pill button in the kind's color, filled while `active`: "<icon>  <text>". Returns true
// when clicked. Chips in a row go on one line with SameLine between them.
inline bool KindChip(AssetKind kind, bool active, const std::string& text) {
    const AssetStyle style = StyleOf(kind);
    const auto& palette = Editor::Palette();
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, ImGui::GetFrameHeight() * 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Button, active ? style.color : palette.WithAlpha(style.color, 0.16f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active ? style.color : palette.WithAlpha(style.color, 0.30f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, style.color);
    ImGui::PushStyleColor(ImGuiCol_Text, active ? palette.TextOnAccent : style.color);
    const bool clicked = ImGui::Button((std::string(style.icon) + "  " + text + "##chip" + style.label).c_str());
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();
    return clicked;
}

// Re-accents everything drawn until PopKindTheme in the kind's color instead of the theme's
// accent: selected rows, checkmarks, sliders, resize bars, text selection. For an asset's own
// editor (its ContentPane), so it reads as that kind throughout.
inline void PushKindTheme(AssetKind kind) {
    const ImVec4 color = StyleOf(kind).color;
    const auto& palette = Editor::Palette();
    ImGui::PushStyleColor(ImGuiCol_Header, palette.WithAlpha(color, 0.28f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, palette.WithAlpha(color, 0.20f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, palette.WithAlpha(color, 0.36f));
    ImGui::PushStyleColor(ImGuiCol_CheckMark, color);
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, color);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, color);
    ImGui::PushStyleColor(ImGuiCol_SeparatorHovered, color);
    ImGui::PushStyleColor(ImGuiCol_SeparatorActive, color);
    ImGui::PushStyleColor(ImGuiCol_ResizeGripHovered, palette.WithAlpha(color, 0.28f));
    ImGui::PushStyleColor(ImGuiCol_ResizeGripActive, color);
    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, palette.WithAlpha(color, 0.35f));
    ImGui::PushStyleColor(ImGuiCol_NavCursor, color);
    ImGui::PushStyleColor(ImGuiCol_DragDropTarget, color);
}
inline void PopKindTheme() { ImGui::PopStyleColor(13); }

// A SectionHeader in the kind's color, for headings inside that kind's own editor.
inline void KindSectionHeader(AssetKind kind, const char* label) {
    const ImVec4 color = StyleOf(kind).color;
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::PushStyleColor(ImGuiCol_Separator, Editor::Palette().WithAlpha(color, 0.45f));
    ImGui::SeparatorText(label);
    ImGui::PopStyleColor(2);
}

// A document's settings screen (scene or prefab), washed in its kind's color under a solid
// banner "<icon>  <title>". Pair with EndKindSettings.
inline void BeginKindSettings(AssetKind kind, const char* id, const std::string& title) {
    const AssetStyle style = StyleOf(kind);
    const auto& palette = Editor::Palette();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, palette.WithAlpha(style.color, 0.10f));
    ImGui::BeginChild(id, ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleColor();

    const ImGuiStyle& imStyle = ImGui::GetStyle();
    const ImVec2 windowPos = ImGui::GetWindowPos();
    const float height = ImGui::GetFrameHeight() + imStyle.FramePadding.y * 2.0f;
    ImGui::GetWindowDrawList()->AddRectFilled(windowPos, ImVec2(windowPos.x + ImGui::GetWindowWidth(), windowPos.y + height),
                                              palette.ToU32(style.color));
    ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, windowPos.y + (height - ImGui::GetTextLineHeight()) * 0.5f));
    ImGui::PushStyleColor(ImGuiCol_Text, palette.TextOnAccent);
    ImGui::Text("%s  %s", style.icon, title.c_str());
    ImGui::PopStyleColor();
    ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, windowPos.y + height + imStyle.ItemSpacing.y));
}
inline void EndKindSettings() { ImGui::EndChild(); }

}  // namespace Elysium
