#include "AssetEditor.h"
#include "Core/Prefab.h"
#include "Core/Audio.h"
#include "Core/Graphics.h"
#include "Core/Asset.h"
#include "Core/Script.h"
#include "Core/Shader.h"
#include "Core/Sprite.h"
#include "Editor/Widgets/AssetField.h"
#include "Editor/Style/AssetStyle.h"
#include "Editor/Widgets/Widgets.h"
#include "Interfaces/IApplicationService.h"
#include "Interfaces/IAssetService.h"
#include "Editor/EditorApplication.h"
#include <tinyxml2.h>
#include "Interfaces/ITaskService.h"
#include "Core/Path.h"
#include <algorithm>
#include <cctype>

namespace Elysium {

namespace fs = std::filesystem;
using namespace Services;

AssetEditor::AssetEditor(EditorApplication& editor) : Editor(editor, Title) {}

namespace {

ImVec4 Darken(const ImVec4& color, float factor) {
    return ImVec4(color.x * factor, color.y * factor, color.z * factor, color.w);
}

std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

DiskCache ScanDiskCache(const fs::path& rootPath) {
    DiskCache cache;
    for (const AssetKindFolder& type : AssetKindFolders()) {
        std::error_code ec;
        const fs::path folder = rootPath / type.folder;
        if (!fs::is_directory(folder, ec)) continue;
        cache[""].push_back({folder, type.folder, AssetKind::Folder});

        for (auto it = fs::recursive_directory_iterator(folder, ec); !ec && it != fs::recursive_directory_iterator();
             it.increment(ec)) {
            if (it->path().filename().string().starts_with(".")) {
                it.disable_recursion_pending();
                continue;
            }
            DiskFile file{it->path(), fs::relative(it->path(), rootPath, ec).generic_string(), AssetKind::Folder};
            if (!it->is_directory(ec)) {
                const std::string ext = ToLower(it->path().extension().string());
                if (std::find(type.extensions.begin(), type.extensions.end(), ext) == type.extensions.end()) continue;
                file.kind = type.kind;
                file.size = it->file_size(ec);
            }
            const std::string parent = fs::path(file.relativePath).parent_path().generic_string();
            cache[parent].push_back(std::move(file));
        }
    }

    // Type folders at the root stay in convention order; below that, folders first, then
    // files, alphabetically (case-insensitive) within each group.
    for (auto& [parent, files] : cache) {
        if (parent.empty()) continue;
        std::sort(files.begin(), files.end(), [](const DiskFile& a, const DiskFile& b) {
            const bool aDir = a.kind == AssetKind::Folder, bDir = b.kind == AssetKind::Folder;
            if (aDir != bDir) return aDir;
            return ToLower(a.path.filename().string()) < ToLower(b.path.filename().string());
        });
    }
    return cache;
}

std::string FormatSize(uintmax_t bytes) {
    char text[32];
    if (bytes < 1024) snprintf(text, sizeof(text), "%u B", (unsigned)bytes);
    else if (bytes < 1024 * 1024) snprintf(text, sizeof(text), "%.1f KB", bytes / 1024.0);
    else snprintf(text, sizeof(text), "%.1f MB", bytes / (1024.0 * 1024.0));
    return text;
}

// `text` cut down with an ellipsis to fit `width`.
std::string FitText(const std::string& text, float width) {
    if (ImGui::CalcTextSize(text.c_str()).x <= width) return text;
    std::string fitted = text;
    while (!fitted.empty() && ImGui::CalcTextSize((fitted + "...").c_str()).x > width) fitted.pop_back();
    return fitted + "...";
}

// `icon` at `size`, centered on `center`.
void DrawIcon(ImDrawList* drawList, const char* icon, float size, ImVec2 center, ImU32 color) {
    const ImVec2 extent = ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, icon);
    drawList->AddText(ImGui::GetFont(), size, ImVec2(center.x - extent.x * 0.5f, center.y - extent.y * 0.5f), color, icon);
}

// The layered file icon: a page in the kind's color with a folded corner and the kind's
// icon, darker, in the middle. Folders are just a large folder glyph.
void DrawFileIcon(ImDrawList* drawList, ImVec2 min, ImVec2 max, AssetKind kind, bool dim) {
    const AssetStyle style = StyleOf(kind);
    const float alpha = dim ? 0.55f : 1.0f;
    const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
    const float height = max.y - min.y;
    if (kind == AssetKind::Folder) {
        DrawIcon(drawList, ICON_FA_FOLDER, height * 0.85f, center, EditorStyle::Palette::ToU32(style.color));
        return;
    }

    const float width = height * 0.78f;
    const ImVec2 a(center.x - width * 0.5f, min.y), b(center.x + width * 0.5f, max.y);
    const float fold = width * 0.3f;
    const ImU32 page = EditorStyle::Palette::ToU32(EditorStyle::Palette::WithAlpha(style.color, alpha));
    const ImU32 shade = EditorStyle::Palette::ToU32(EditorStyle::Palette::WithAlpha(Darken(style.color, 0.55f), alpha));
    const float rounding = width * 0.08f;

    // Page outline minus the folded corner.
    drawList->PathLineTo(ImVec2(a.x + rounding, a.y));
    drawList->PathLineTo(ImVec2(b.x - fold, a.y));
    drawList->PathLineTo(ImVec2(b.x, a.y + fold));
    drawList->PathArcTo(ImVec2(b.x - rounding, b.y - rounding), rounding, 0.0f, IM_PI * 0.5f);
    drawList->PathArcTo(ImVec2(a.x + rounding, b.y - rounding), rounding, IM_PI * 0.5f, IM_PI);
    drawList->PathArcTo(ImVec2(a.x + rounding, a.y + rounding), rounding, IM_PI, IM_PI * 1.5f);
    drawList->PathFillConvex(page);
    // The fold.
    drawList->AddTriangleFilled(ImVec2(b.x - fold, a.y), ImVec2(b.x - fold, a.y + fold), ImVec2(b.x, a.y + fold),
                                EditorStyle::Palette::ToU32(EditorStyle::Palette::WithAlpha(Darken(style.color, 0.75f), alpha)));

    DrawIcon(drawList, style.icon, width * 0.5f, ImVec2(center.x, center.y + fold * 0.25f), shade);
}

}  // namespace

void AssetEditor::RefreshDiskCacheIfDue() {
    // Discovery & Polling — rooted at the current project's asset root, not the
    // engine's own Assets/ (which holds editor-only resources loaded via
    // PathRoot::Engine and shouldn't show up as browsable project content).
    if (rootPath_.empty()) {
        const std::string& assetsRoot = Path::GetAssetsRoot();
        if (fs::exists(assetsRoot)) rootPath_ = fs::canonical(assetsRoot);
    }
    if (rootPath_.empty()) return;

    // Scan runs on TaskService's worker thread — a synchronous recursive scan here
    // can stall the whole app for a frame or more (e.g. on a OneDrive-synced folder),
    // which reads as periodic freezes during unrelated interactions like gizmo dragging.
    const double now = services_.Get<IApplicationService>().GetTime();
    if (refreshInFlight_ || now - lastRefreshTime_ <= refreshInterval_) return;
    refreshInFlight_ = true;
    lastRefreshTime_ = now;

    fs::path rootPath = rootPath_;
    services_.Get<ITaskService>()
        .Submit<DiskCache>(std::function<DiskCache()>(
            [rootPath]() { return ScanDiskCache(rootPath); }))
        .Then([this](const DiskCache& cache) {
            directoryCache_ = cache;
            refreshInFlight_ = false;
            // The folder we're in may have been deleted or renamed.
            if (!currentFolder_.empty() && !directoryCache_.count(currentFolder_)) {
                bool exists = false;
                const std::string parent = fs::path(currentFolder_).parent_path().generic_string();
                auto it = directoryCache_.find(parent);
                if (it != directoryCache_.end()) {
                    for (const auto& file : it->second) exists |= file.relativePath == currentFolder_;
                }
                if (!exists) currentFolder_.clear();
            }
        });
}

void AssetEditor::NavigateTo(const std::string& folder) {
    if (folder == currentFolder_) return;
    back_.push_back(currentFolder_);
    forward_.clear();
    currentFolder_ = folder;
}

void AssetEditor::Draw() {
    RefreshDiskCacheIfDue();

    if (BeginWindow()) {
        if (rootPath_.empty()) {
            EmptyState("No project asset folder");
        } else {
            DrawNavBar();
            DrawChips();
            ImGui::Separator();

            LoadedAssets loaded;
            for (const auto& [path, asset] : services_.Get<IAssetService>().GetAllAssets()) {
                if (asset->IsLoaded()) loaded[path.GetRelativePath()] = asset.get();
            }

            ImGui::BeginChild("AssetView");
            const auto files = VisibleFiles();
            if (files.empty()) EmptyState(searchBuffer_[0] ? "No matching assets" : "Empty folder");
            else if (gridView_) DrawGrid(files, loaded);
            else DrawList(files, loaded);

            // Right-click on empty space: create a file here. Inside a kind's folder it's that
            // kind; at the root, any kind (into its own folder).
            if (ImGui::BeginPopupContextWindow("FolderCtx", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
                const std::string top = currentFolder_.substr(0, currentFolder_.find('/'));
                const AssetKindFolder* here = nullptr;
                for (const auto& folder : AssetKindFolders()) {
                    if (top == folder.folder) here = &folder;
                }
                if (!here) {
                    if (ImGui::MenuItem(ICON_FA_FILE_CIRCLE_PLUS "  Create File...")) createDialog_.OpenNew();
                } else if (here->kind != AssetKind::Sound && here->kind != AssetKind::Texture && here->kind != AssetKind::Model) {
                    const AssetStyle style = StyleOf(here->kind);
                    ImGui::PushStyleColor(ImGuiCol_Text, style.color);
                    const bool create = ImGui::MenuItem((std::string(style.icon) + "  Create " + style.label + "...").c_str());
                    ImGui::PopStyleColor();
                    if (create) createDialog_.OpenNew(here->kind, currentFolder_);
                } else {
                    ImGui::TextDisabled("%ss are added from outside the editor", StyleOf(here->kind).label);
                }
                ImGui::EndPopup();
            }
            ImGui::EndChild();

            if (const auto result = createDialog_.Draw()) {
                editor_.CreateAsset(result->kind, result->fullPath);
                lastRefreshTime_ = -1e9;  // rescan now so the new file shows up
            }
        }
    }
    EndWindow();
}

void AssetEditor::DrawNavBar() {
    ImGui::BeginDisabled(back_.empty());
    if (IconButton(ICON_FA_ARROW_LEFT, "Back")) {
        forward_.push_back(currentFolder_);
        currentFolder_ = back_.back();
        back_.pop_back();
    }
    ImGui::EndDisabled();
    ImGui::SameLine(0, 0);
    ImGui::BeginDisabled(forward_.empty());
    if (IconButton(ICON_FA_ARROW_RIGHT, "Forward")) {
        back_.push_back(currentFolder_);
        currentFolder_ = forward_.back();
        forward_.pop_back();
    }
    ImGui::EndDisabled();
    ImGui::SameLine(0, 0);
    ImGui::BeginDisabled(currentFolder_.empty());
    if (IconButton(ICON_FA_ARROW_UP, "Up")) NavigateTo(fs::path(currentFolder_).parent_path().generic_string());
    ImGui::EndDisabled();
    ImGui::SameLine(0, 0);
    if (IconButton(ICON_FA_HOUSE, "Project root")) NavigateTo("");

    // Location: each segment of the current folder is a button back to it.
    ImGui::SameLine();
    const auto& palette = Palette();
    ImGui::PushStyleColor(ImGuiCol_Button, palette.WithAlpha(palette.Surface0, 0.0f));
    std::string target;
    std::string clicked;
    bool anyClicked = ImGui::Button("Project##crumb");
    if (!currentFolder_.empty()) {
        for (const auto& segment : fs::path(currentFolder_)) {
            target = target.empty() ? segment.generic_string() : target + "/" + segment.generic_string();
            ImGui::SameLine(0, 0);
            ImGui::TextDisabled("/");
            ImGui::SameLine(0, 0);
            ImGui::PushID(target.c_str());
            if (ImGui::Button(segment.generic_string().c_str())) {
                clicked = target;
                anyClicked = true;
            }
            ImGui::PopID();
        }
    }
    ImGui::PopStyleColor();
    if (anyClicked) NavigateTo(clicked);

    // Right side: search and the view toggle.
    const float searchWidth = 160.0f;
    const float toggleWidth = ImGui::GetFrameHeight() * 2.0f;
    AlignRight(searchWidth + toggleWidth + ImGui::GetStyle().ItemSpacing.x);
    SearchField("##AssetSearch", searchBuffer_, sizeof(searchBuffer_), searchWidth);
    ImGui::SameLine();
    if (ToggleIconButton(ICON_FA_TABLE_CELLS_LARGE, gridView_, "Grid view")) gridView_ = true;
    ImGui::SameLine(0, 0);
    if (ToggleIconButton(ICON_FA_LIST, !gridView_, "List view")) gridView_ = false;
}

void AssetEditor::DrawChips() {
    std::unordered_map<int, int> counts;
    for (const auto& [folder, files] : directoryCache_) {
        for (const auto& file : files) ++counts[(int)file.kind];
    }

    for (const AssetKindFolder& type : AssetKindFolders()) {
        // Lit while browsing anywhere inside the type's folder.
        const bool active = currentFolder_ == type.folder || currentFolder_.starts_with(std::string(type.folder) + "/");
        const std::string text = std::string(StyleOf(type.kind).label) + "s  " + std::to_string(counts[(int)type.kind]);
        if (KindChip(type.kind, active, text)) NavigateTo(type.folder);
        ImGui::SameLine();
    }
    ImGui::NewLine();
}

std::vector<const DiskFile*> AssetEditor::VisibleFiles() const {
    std::vector<const DiskFile*> files;
    if (!searchBuffer_[0]) {
        auto it = directoryCache_.find(currentFolder_);
        if (it != directoryCache_.end()) {
            for (const auto& file : it->second) files.push_back(&file);
        }
        return files;
    }
    for (const auto& [folder, folderFiles] : directoryCache_) {
        for (const auto& file : folderFiles) {
            if (file.kind == AssetKind::Folder) continue;
            if (!MatchesSearch(file.relativePath, searchBuffer_)) continue;
            files.push_back(&file);
        }
    }
    std::sort(files.begin(), files.end(), [](const DiskFile* a, const DiskFile* b) {
        return ToLower(a->path.filename().string()) < ToLower(b->path.filename().string());
    });
    return files;
}

void AssetEditor::DrawGrid(const std::vector<const DiskFile*>& files, const LoadedAssets& loaded) {
    const float iconSize = 56.0f;
    const float padding = 6.0f;
    const ImVec2 cell(iconSize + 28.0f, iconSize + ImGui::GetTextLineHeight() + padding * 3.0f);
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const int columns = std::max(1, (int)((ImGui::GetContentRegionAvail().x + spacing) / (cell.x + spacing)));
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    for (size_t i = 0; i < files.size(); ++i) {
        const DiskFile& file = *files[i];
        auto found = loaded.find(file.relativePath);
        IAsset* asset = found != loaded.end() ? found->second : nullptr;

        if (i % columns != 0) ImGui::SameLine();
        ImGui::PushID(file.relativePath.c_str());
        const ImVec2 min = ImGui::GetCursorScreenPos();
        const bool clicked = ImGui::InvisibleButton("##cell", cell, ImGuiButtonFlags_MouseButtonLeft);
        // InvisibleButton reports a click on release, so it never sees the double-click itself.
        const bool doubleClicked = ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
        const ImVec2 max(min.x + cell.x, min.y + cell.y);

        const bool selected = selectedFile_ == file.relativePath;
        if (selected || ImGui::IsItemHovered()) {
            drawList->AddRectFilled(min, max, Palette().ToU32(selected ? Palette().AccentSoft : Palette().Surface0),
                                    ImGui::GetStyle().FrameRounding);
        }
        DrawFileIcon(drawList, ImVec2(min.x, min.y + padding), ImVec2(max.x, min.y + padding + iconSize), file.kind,
                     false);

        const std::string name = FitText(file.path.filename().string(), cell.x - padding);
        const float textWidth = ImGui::CalcTextSize(name.c_str()).x;
        const bool dim = file.kind != AssetKind::Folder && !asset;
        drawList->AddText(ImVec2(min.x + (cell.x - textWidth) * 0.5f, min.y + iconSize + padding * 2.0f),
                          Palette().ToU32(dim ? Palette().TextMuted : Palette().Text), name.c_str());

        HandleItem(file, asset, clicked, doubleClicked);
        ImGui::PopID();
    }
}

void AssetEditor::DrawList(const std::vector<const DiskFile*>& files, const LoadedAssets& loaded) {
    const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV;
    if (!ImGui::BeginTable("AssetList", 4, flags)) return;
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 70.0f);
    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableHeadersRow();

    const bool flat = searchBuffer_[0] != '\0';
    for (const DiskFile* filePtr : files) {
        const DiskFile& file = *filePtr;
        auto found = loaded.find(file.relativePath);
        IAsset* asset = found != loaded.end() ? found->second : nullptr;
        const AssetStyle style = StyleOf(file.kind);

        ImGui::PushID(file.relativePath.c_str());
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        const std::string name = flat ? file.relativePath : file.path.filename().string();
        // A bare Selectable, so the tooltip and context menu attach to the row itself.
        const bool clicked = ImGui::Selectable("##row", selectedFile_ == file.relativePath,
                                               ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick |
                                                   ImGuiSelectableFlags_AllowOverlap,
                                               ImVec2(0, ImGui::GetFrameHeight()));
        HandleItem(file, asset, clicked, clicked && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left));
        ImGui::SameLine(0, 0);
        ImGui::AlignTextToFramePadding();
        ColoredText(style.color, style.icon);
        ImGui::SameLine();
        ImGui::TextUnformatted(name.c_str());
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", style.label);
        ImGui::TableNextColumn();
        if (file.kind != AssetKind::Folder) ImGui::TextDisabled("%s", FormatSize(file.size).c_str());
        ImGui::TableNextColumn();
        if (file.kind != AssetKind::Folder) {
            if (asset) ColoredText(Palette().Success, "Loaded");
            else ImGui::TextDisabled("-");
        }
        ImGui::PopID();
    }
    ImGui::EndTable();
}

void AssetEditor::Open(const DiskFile& file) {
    if (file.kind == AssetKind::Folder) NavigateTo(file.relativePath);
    else editor_.OpenAsset(Path(file.relativePath).GetFullPath());
}

void AssetEditor::HandleItem(const DiskFile& file, IAsset* asset, bool clicked, bool doubleClicked) {
    if (clicked || doubleClicked) selectedFile_ = file.relativePath;
    if (doubleClicked) Open(file);
    if (file.kind == AssetKind::Folder) {
        ItemTooltip(file.relativePath.c_str());
        return;
    }
    AssetDragSource(file.kind, file.relativePath);
    const std::string tooltip = file.relativePath + (asset ? "\nLoaded" : "\nNot loaded - right-click to load");
    ItemTooltip(tooltip.c_str());

    if (!ImGui::BeginPopupContextItem("AssetCtx")) return;
    selectedFile_ = file.relativePath;
    auto& assetService = services_.Get<IAssetService>();
    const Path path(file.relativePath);
    if (file.kind != AssetKind::Sound && ImGui::MenuItem(ICON_FA_PEN_TO_SQUARE "  Open")) Open(file);
    if (file.kind == AssetKind::Prefab) {
        auto& editor = editor_;
        ImGui::BeginDisabled(!editor.GetWorld());
        if (ImGui::MenuItem(ICON_FA_CUBE "  Place in Viewport")) editor.InstantiatePrefab(path.GetFullPath());
        ImGui::EndDisabled();
    }
    ImGui::Separator();
    if (file.kind != AssetKind::Scene) {
        if (!asset) {
            if (ImGui::MenuItem("Load")) {
                switch (file.kind) {
                    case AssetKind::Sound: assetService.LoadAsset<Sound>(path); break;
                    case AssetKind::Prefab: assetService.LoadAsset<Prefab>(path); break;
                    case AssetKind::Sprite: assetService.LoadAsset<Sprite>(path); break;
                    case AssetKind::Script: assetService.LoadAsset<Script>(path); break;
                    case AssetKind::Texture: assetService.LoadAsset<Texture>(path); break;
                    // ShaderAsset treats its path as the fragment shader and picks up the
                    // sibling .vs itself, if there is one.
                    case AssetKind::Shader: assetService.LoadAsset<Shader>(path); break;
                    case AssetKind::Model: assetService.LoadAsset<Model>(path); break;
                    default: break;
                }
            }
        } else {
            if (ImGui::MenuItem(ICON_FA_ROTATE_LEFT "  Reload")) assetService.ReloadAsset(asset);
            if (ImGui::MenuItem("Unload")) asset->Unload();
        }
    }
    ImGui::EndPopup();
}

} // namespace Elysium
