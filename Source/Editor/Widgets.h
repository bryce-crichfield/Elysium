#pragma once

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

#include "imgui.h"
#include "extras/IconsFontAwesome6.h"

#include "Core/Value.h"
#include "Editor/Palette.h"
#include "Editor/Theme.h"

// Building blocks shared by every editor panel and component inspector, so they all lay
// out the same way: a fixed label column, full-width widgets, muted secondary text.
namespace Elysium {

// Starts an inspector row: `label` in the label column, then sizes the next widget to
// fill the rest of the row. Muted labels mark a value still at its default.
inline void PropertyLabel(const char* label, bool muted = false) {
    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, muted ? Editor::Palette::TextMuted : Editor::Palette::Text);
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    ImGui::SameLine(Editor::Theme::LabelColumnWidth);
    ImGui::SetNextItemWidth(-FLT_MIN);
}

// A read-only inspector row. The muted label marks it as not editable.
inline void ReadOnlyRow(const char* label, const char* value) {
    PropertyLabel(label, true);
    ImGui::TextUnformatted(value);
}

// A labelled group heading inside a panel or inspector.
inline void SectionHeader(const char* label) {
    ImGui::PushStyleColor(ImGuiCol_Text, Editor::Palette::TextMuted);
    ImGui::SeparatorText(label);
    ImGui::PopStyleColor();
}

inline void MutedText(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, Editor::Palette::TextMuted);
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}

inline void ColoredText(const ImVec4& color, const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
}

// Placeholder for a panel with nothing to show, centered in the space left.
inline void EmptyState(const char* message) {
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const ImVec2 size = ImGui::CalcTextSize(message);
    const ImVec2 cursor = ImGui::GetCursorPos();
    ImGui::SetCursorPos(ImVec2(cursor.x + std::max(0.0f, (avail.x - size.x) * 0.5f),
                               cursor.y + std::max(0.0f, (avail.y - size.y) * 0.5f)));
    ImGui::TextDisabled("%s", message);
}

inline void ItemTooltip(const char* text) {
    if (text && *text) ImGui::SetItemTooltip("%s", text);
}

// A frameless button showing only an icon, with a hover tooltip. Frame height, so it
// lines up with the inputs beside it.
inline bool IconButton(const char* icon, const char* tooltip = nullptr) {
    ImGui::PushStyleColor(ImGuiCol_Button, Editor::Palette::WithAlpha(Editor::Palette::Surface0, 0.0f));
    const bool pressed = ImGui::Button(icon);
    ImGui::PopStyleColor();
    ItemTooltip(tooltip);
    return pressed;
}

// A button with the accent fill, for the one primary action in a toolbar.
inline bool PrimaryButton(const char* label, const ImVec2& size = ImVec2(0, 0)) {
    ImGui::PushStyleColor(ImGuiCol_Button, Editor::Palette::Accent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Editor::Palette::AccentHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Editor::Palette::AccentActive);
    ImGui::PushStyleColor(ImGuiCol_Text, Editor::Palette::Crust);
    const bool pressed = ImGui::Button(label, size);
    ImGui::PopStyleColor(4);
    return pressed;
}

// A search box with a magnifier hint. `width` <= 0 fills the row. Returns true when edited.
inline bool SearchField(const char* id, char* buffer, size_t bufferSize, float width = -FLT_MIN) {
    ImGui::SetNextItemWidth(width);
    return ImGui::InputTextWithHint(id, ICON_FA_MAGNIFYING_GLASS "  Search", buffer, bufferSize);
}

// Case-insensitive substring test backing every SearchField. An empty query matches all.
inline bool MatchesSearch(const std::string& text, const char* query) {
    if (!query || !*query) return true;
    auto equal = [](unsigned char a, unsigned char b) { return std::tolower(a) == std::tolower(b); };
    const char* queryEnd = query + std::strlen(query);
    return std::search(text.begin(), text.end(), query, queryEnd, equal) != text.end();
}

// How an entity is shown in lists: its name, or "Entity <id>" when it has none.
inline std::string EntityLabel(const std::string& name, size_t entity) {
    return name.empty() ? "Entity " + std::to_string(entity) : name;
}

// Right-aligns the next `width` pixels of the current row.
inline void AlignRight(float width) {
    const float x = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - width;
    ImGui::SameLine(std::max(x, ImGui::GetCursorPosX()));
}

// A frame-height, full-width selectable list row: a colored icon, the label, and optional
// muted text pinned right, and a hover tooltip. Returns true when clicked;
// ImGui::IsMouseDoubleClicked after it tells a double-click apart.
inline bool ListRow(const char* id, bool selected, const char* icon, const ImVec4& iconColor, const char* label,
                    const char* trailing = nullptr, const char* tooltip = nullptr) {
    ImGui::PushID(id);
    const float startX = ImGui::GetCursorPosX();
    const bool clicked = ImGui::Selectable("##row", selected,
                                           ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_AllowOverlap,
                                           ImVec2(0, ImGui::GetFrameHeight()));
    ItemTooltip(tooltip);
    ImGui::SameLine(startX + ImGui::GetStyle().FramePadding.x);
    ImGui::AlignTextToFramePadding();
    ColoredText(iconColor, icon);
    ImGui::SameLine();
    ImGui::TextUnformatted(label);
    if (trailing && *trailing) {
        AlignRight(ImGui::CalcTextSize(trailing).x + ImGui::GetStyle().FramePadding.x);
        ImGui::TextDisabled("%s", trailing);
    }
    ImGui::PopID();
    return clicked;
}

// Expand-all / collapse-all for a panel of headers or tree nodes. A click sets `request`;
// the panel passes it to ApplyOpenRequest before each node that frame, then resets it.
constexpr const char* kExpandAllIcon = ICON_FA_ANGLES_DOWN;
constexpr const char* kCollapseAllIcon = ICON_FA_ANGLES_UP;
inline void ExpandCollapseButtons(std::optional<bool>& request) {
    if (IconButton(kExpandAllIcon, "Expand all")) request = true;
    ImGui::SameLine();
    if (IconButton(kCollapseAllIcon, "Collapse all")) request = false;
}
inline void ApplyOpenRequest(const std::optional<bool>& request) {
    if (request) ImGui::SetNextItemOpen(*request);
}

// A collapsible section header, dimmed while `active` is off. AllowOverlap lets buttons
// sit on its right edge. Wrap the body, when open, in Begin/EndSectionBody.
inline bool CollapsingSection(const char* label, bool active = true, ImGuiTreeNodeFlags flags = 0) {
    ImGui::PushStyleColor(ImGuiCol_Text, active ? Editor::Palette::Text : Editor::Palette::TextMuted);
    const bool open = ImGui::CollapsingHeader(label, flags | ImGuiTreeNodeFlags_AllowOverlap);
    ImGui::PopStyleColor();
    return open;
}
inline void BeginSectionBody() { ImGui::Indent(Editor::Theme::ItemInnerSpacing.x); }
inline void EndSectionBody() {
    ImGui::Unindent(Editor::Theme::ItemInnerSpacing.x);
    ImGui::Spacing();
}

// Width of each of `count` buttons sharing the rest of the row.
inline float SharedButtonWidth(int count) {
    return (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * (count - 1)) / count;
}

// Width a button showing `label` takes, for AlignRight and fill-minus-button widths.
inline float ButtonWidth(const char* label) {
    return ImGui::CalcTextSize(label, nullptr, true).x + ImGui::GetStyle().FramePadding.x * 2.0f;
}

// Width ExpandCollapseButtons takes, including the spacing before it.
inline float ExpandCollapseWidth() {
    return ButtonWidth(kExpandAllIcon) + ButtonWidth(kCollapseAllIcon) + ImGui::GetStyle().ItemSpacing.x * 2.0f;
}

// One widget for a Value, picked by its type; vec3/vec4 use a color picker when asColor.
// Returns true when edited.
inline bool InspectValue(const char* id, Value& value, bool asColor = false) {
    if (value.Is<bool>()) {
        bool v = value.As<bool>();
        if (!ImGui::Checkbox(id, &v)) return false;
        value = Value(v);
    } else if (value.Is<int>()) {
        int v = value.As<int>();
        if (!ImGui::DragInt(id, &v)) return false;
        value = Value(v);
    } else if (value.Is<float>()) {
        float v = value.As<float>();
        if (!ImGui::DragFloat(id, &v, 0.05f)) return false;
        value = Value(v);
    } else if (value.Is<Vector2>()) {
        Vector2 v = value.As<Vector2>();
        if (!ImGui::DragFloat2(id, &v.x, 0.05f)) return false;
        value = Value(v);
    } else if (value.Is<Vector3>()) {
        Vector3 v = value.As<Vector3>();
        if (!(asColor ? ImGui::ColorEdit3(id, &v.x) : ImGui::DragFloat3(id, &v.x, 0.05f))) return false;
        value = Value(v);
    } else {
        Vector4 v = value.As<Vector4>();
        if (!(asColor ? ImGui::ColorEdit4(id, &v.x) : ImGui::DragFloat4(id, &v.x, 0.05f))) return false;
        value = Value(v);
    }
    return true;
}

// A Value row: label (muted while at `fallback`), widget, and a reset button enabled off
// the default. Returns true when edited or reset; `value` holds the result.
inline bool InspectValueRow(const std::string& name, Value& value, const Value& fallback, bool isDefault,
                            bool asColor = false) {
    ImGui::PushID(name.c_str());
    PropertyLabel(name.c_str(), isDefault);
    ImGui::SetNextItemWidth(-(ButtonWidth(ICON_FA_ROTATE_LEFT) + ImGui::GetStyle().ItemSpacing.x));
    bool changed = InspectValue("##value", value, asColor);
    ImGui::SameLine();
    ImGui::BeginDisabled(isDefault);
    if (IconButton(ICON_FA_ROTATE_LEFT, "Reset to default")) { value = fallback; changed = true; }
    ImGui::EndDisabled();
    ImGui::PopID();
    return changed;
}

// Combo over `options` plus a leading "<None>" (the empty path). Returns true when changed.
inline bool InspectPathCombo(const char* id, std::string& path, const std::vector<std::string>& options) {
    bool changed = false;
    if (ImGui::BeginCombo(id, path.empty() ? "<None>" : path.c_str())) {
        for (size_t i = 0; i <= options.size(); ++i) {
            const std::string option = i == 0 ? "" : options[i - 1];
            const std::string label = (i == 0 ? std::string("<None>") : option) + "##" + std::to_string(i);
            const bool isSelected = option == path;
            if (ImGui::Selectable(label.c_str(), isSelected) && !isSelected) { path = option; changed = true; }
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return changed;
}

}  // namespace Elysium
