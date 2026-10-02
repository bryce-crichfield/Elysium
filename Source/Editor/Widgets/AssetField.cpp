#include "Editor/Widgets/AssetField.h"

#include <algorithm>
#include <filesystem>
#include <map>

#include "imgui_internal.h"

#include "Core/Path.h"
#include "Editor/Style/AssetStyle.h"
#include "Editor/Widgets/Widgets.h"

namespace Elysium {

namespace {

constexpr double kRescanSeconds = 2.0;

struct KindScan {
    std::vector<std::string> paths;
    double time = -1e9;
};

const AssetKindFolder* FolderOf(AssetKind kind) {
    for (const auto& folder : AssetKindFolders()) {
        if (folder.kind == kind) return &folder;
    }
    return nullptr;
}

std::string FileName(const std::string& path) { return std::filesystem::path(path).stem().string(); }

}  // namespace

void AssetDragSource(AssetKind kind, const std::string& relativePath) {
    if (!ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) return;
    ImGui::SetDragDropPayload(kAssetDragPayload, relativePath.c_str(), relativePath.size() + 1);
    const AssetStyle style = StyleOf(kind);
    ColoredText(style.color, style.icon);
    ImGui::SameLine();
    ImGui::TextUnformatted(FileName(relativePath).c_str());
    ImGui::EndDragDropSource();
}

std::optional<std::string> AcceptAssetDrop(AssetKind kind) {
    const ImGuiPayload* peek = ImGui::GetDragDropPayload();
    if (!peek || !peek->IsDataType(kAssetDragPayload)) return std::nullopt;
    if (AssetKindOf(static_cast<const char*>(peek->Data)) != kind) return std::nullopt;
    const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kAssetDragPayload);
    if (!payload) return std::nullopt;
    return std::string(static_cast<const char*>(payload->Data));
}

const std::vector<std::string>& DiscoverAssets(AssetKind kind) {
    static std::map<AssetKind, KindScan> scans;
    KindScan& scan = scans[kind];
    const double now = ImGui::GetTime();
    if (now - scan.time < kRescanSeconds) return scan.paths;
    scan.time = now;
    scan.paths.clear();

    namespace fs = std::filesystem;
    const AssetKindFolder* folder = FolderOf(kind);
    if (!folder) return scan.paths;
    std::error_code ec;
    const fs::path root(Path::GetAssetsRoot());
    for (auto it = fs::recursive_directory_iterator(root / folder->folder, ec); !ec && it != fs::recursive_directory_iterator();
         it.increment(ec)) {
        if (it->is_directory(ec)) continue;
        const std::string relative = fs::relative(it->path(), root, ec).generic_string();
        if (AssetKindOf(relative) == kind) scan.paths.push_back(relative);
    }
    std::sort(scan.paths.begin(), scan.paths.end());
    return scan.paths;
}

bool AssetField(const char* id, AssetKind kind, std::string& path, const std::function<bool(const std::string&)>& filter) {
    const AssetStyle style = StyleOf(kind);
    const auto& palette = Editor::Palette();
    const auto& all = DiscoverAssets(kind);
    const bool missing = !path.empty() && std::find(all.begin(), all.end(), path) == all.end();
    bool changed = false;

    ImGui::PushID(id);
    // The field's rect, known before the combo opens its popup, for the drop target.
    const ImVec2 min = ImGui::GetCursorScreenPos();
    const ImRect rect(min, ImVec2(min.x + ImGui::CalcItemWidth(), min.y + ImGui::GetFrameHeight()));

    const ImVec4 edge = missing ? palette.Error : style.color;
    ImGui::PushStyleColor(ImGuiCol_FrameBg, palette.WithAlpha(edge, 0.10f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, palette.WithAlpha(edge, 0.20f));
    ImGui::PushStyleColor(ImGuiCol_Button, palette.WithAlpha(edge, 0.18f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, palette.WithAlpha(edge, 0.30f));
    ImGui::PushStyleColor(ImGuiCol_Border, palette.WithAlpha(edge, 0.55f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    const bool open = ImGui::BeginCombo("##field", "", ImGuiComboFlags_CustomPreview | ImGuiComboFlags_HeightLarge);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(5);

    if (open) {
        static char search[128] = "";
        if (ImGui::IsWindowAppearing()) {
            search[0] = '\0';
            ImGui::SetKeyboardFocusHere();
        }
        SearchField("##search", search, sizeof(search));
        if (ListRow("none", path.empty(), ICON_FA_BAN, palette.TextMuted, "None") && !path.empty()) {
            path.clear();
            changed = true;
            ImGui::CloseCurrentPopup();
        }
        for (const std::string& option : all) {
            if ((filter && !filter(option)) || !MatchesSearch(option, search)) continue;
            // Trailing: the folder inside the kind's, when it's nested.
            const std::string parent = std::filesystem::path(option).parent_path().generic_string();
            const std::string name = FileName(option);
            if (ListRow(option.c_str(), option == path, style.icon, style.color, name.c_str(), parent.c_str(), option.c_str()) &&
                option != path) {
                path = option;
                changed = true;
                ImGui::CloseCurrentPopup();
            }
            if (option == path && ImGui::IsWindowAppearing()) ImGui::SetScrollHereY();
        }
        if (all.empty()) MutedText((std::string("No ") + style.label + "s in the project").c_str());
        ImGui::EndCombo();
    }

    // Preview: the kind's icon in its color, then the asset's name.
    if (ImGui::BeginComboPreview()) {
        if (path.empty()) {
            ColoredText(palette.WithAlpha(style.color, 0.6f), style.icon);
            ImGui::SameLine();
            ImGui::TextDisabled("None (%s)", style.label);
        } else {
            ColoredText(edge, missing ? ICON_FA_TRIANGLE_EXCLAMATION : style.icon);
            ImGui::SameLine();
            ImGui::TextUnformatted(FileName(path).c_str());
        }
        ImGui::EndComboPreview();
    }
    if (!open && ImGui::IsMouseHoveringRect(rect.Min, rect.Max) && !ImGui::IsDragDropActive()) {
        const std::string tip = path.empty() ? std::string(style.label) + " - pick one, or drag one here from Assets"
                                : missing    ? path + "\nNo such " + style.label + " in the project"
                                             : path;
        ImGui::SetTooltip("%s", tip.c_str());
    }

    // Drop: only a file of this kind (and passing the filter) is taken; anything else is
    // outlined in red and refused.
    if (ImGui::BeginDragDropTargetCustom(rect, ImGui::GetID("##drop"))) {
        if (const ImGuiPayload* payload =
                ImGui::AcceptDragDropPayload(kAssetDragPayload, ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect)) {
            const std::string dropped(static_cast<const char*>(payload->Data));
            const bool valid = AssetKindOf(dropped) == kind && (!filter || filter(dropped));
            ImGui::GetWindowDrawList()->AddRect(rect.Min, rect.Max, palette.ToU32(valid ? style.color : palette.Error),
                                                ImGui::GetStyle().FrameRounding, 0, 2.0f);
            if (!valid) ImGui::SetTooltip("Not a %s", style.label);
            if (valid && payload->IsDelivery() && dropped != path) {
                path = dropped;
                changed = true;
            }
        }
        ImGui::EndDragDropTarget();
    }
    ImGui::PopID();
    return changed;
}

bool AssetFieldRow(const char* label, AssetKind kind, std::string& path, const std::function<bool(const std::string&)>& filter) {
    PropertyLabel(label);
    return AssetField(label, kind, path, filter);
}

}  // namespace Elysium
