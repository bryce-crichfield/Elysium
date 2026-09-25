#include "AssetEditor.h"
#include "Core/Audio.h"
#include "Core/Graphics.h"
#include "Core/Asset.h"
#include "Core/Script.h"
#include "Core/Shader.h"
#include "Core/Sprite.h"
#include "Editor/Widgets.h"
#include "Interfaces/IApplicationService.h"
#include "Interfaces/IAssetService.h"
#include "Interfaces/ITaskService.h"
#include "Core/Path.h"
#include <algorithm>
#include <cctype>

namespace Elysium {

namespace fs = std::filesystem;
using namespace Services;

AssetEditor::AssetEditor(ServiceLocator& services) : Editor(services, Title) {}

namespace {
std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

DiskCache ScanDiskCache(fs::path rootPath) {
    DiskCache cache;
    for (const auto& entry : fs::recursive_directory_iterator(rootPath)) {
        std::string parentPath = entry.path().parent_path().generic_string();

        DiskFile file;
        file.path = entry.path();
        file.relativePath = fs::relative(entry.path(), rootPath).generic_string();
        file.isDirectory = entry.is_directory();

        cache[parentPath].push_back(file);
    }

    // Folders first, then files, alphabetically (case-insensitive) within each group.
    for (auto& [parentPath, files] : cache) {
        std::sort(files.begin(), files.end(), [](const DiskFile& a, const DiskFile& b) {
            if (a.isDirectory != b.isDirectory) return a.isDirectory > b.isDirectory;
            return ToLower(a.path.filename().string()) < ToLower(b.path.filename().string());
        });
    }

    return cache;
}

const char* FileIcon(const fs::path& path) {
    const std::string ext = ToLower(path.extension().string());
    if (ext == ".png" || ext == ".jpg") return ICON_FA_IMAGE;
    if (ext == ".wav" || ext == ".ogg" || ext == ".mp3") return ICON_FA_MUSIC;
    if (ext == ".lua" || ext == ".fs" || ext == ".vs" || ext == ".glsl") return ICON_FA_FILE_CODE;
    return ICON_FA_FILE;
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
        .Submit<DiskCache>(std::function<DiskCache()>([rootPath]() { return ScanDiskCache(rootPath); }))
        .Then([this](const DiskCache& cache) {
            directoryCache_ = cache;
            refreshInFlight_ = false;
        });
}

void AssetEditor::Draw() {
    RefreshDiskCacheIfDue();

    if (BeginWindow()) {
        if (rootPath_.empty()) {
            EmptyState("No project asset folder");
        } else {
            SearchField("##AssetSearch", searchBuffer_, sizeof(searchBuffer_));
            ImGui::Separator();

            LoadedAssets loaded;
            for (const auto& [path, asset] : services_.Get<IAssetService>().GetAllAssets()) {
                if (asset->IsLoaded()) loaded[path.GetRelativePath()] = asset.get();
            }

            ImGui::BeginChild("AssetTree");
            if (searchBuffer_[0] != '\0') DrawSearchResults(loaded);
            else DrawTree(rootPath_, loaded);
            ImGui::EndChild();
        }
    }
    EndWindow();
}

void AssetEditor::DrawTree(const fs::path& currentPath, const LoadedAssets& loaded) {
    auto it = directoryCache_.find(currentPath.generic_string());
    if (it == directoryCache_.end()) return;

    for (const auto& file : it->second) {
        // Keyed by path: two files can share a name in different folders.
        ImGui::PushID(file.relativePath.c_str());

        const std::string name = file.path.filename().string();
        if (file.isDirectory) {
            const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth;
            const bool open = ImGui::TreeNodeEx("##dir", flags, "%s  %s", ICON_FA_FOLDER, name.c_str());
            if (open) {
                DrawTree(file.path, loaded);
                ImGui::TreePop();
            }
        } else {
            DrawFile(file, loaded, name);
        }

        ImGui::PopID();
    }
}

void AssetEditor::DrawSearchResults(const LoadedAssets& loaded) {
    bool any = false;
    for (const auto& [directory, files] : directoryCache_) {
        for (const auto& file : files) {
            if (file.isDirectory || !MatchesSearch(file.relativePath, searchBuffer_)) continue;
            ImGui::PushID(file.relativePath.c_str());
            DrawFile(file, loaded, file.relativePath);
            ImGui::PopID();
            any = true;
        }
    }
    if (!any) EmptyState("No matching assets");
}

// A file row: icon and label, full-strength text once loaded, and a load/reload context menu.
void AssetEditor::DrawFile(const DiskFile& file, const LoadedAssets& loaded, const std::string& label) {
    auto& assetService = services_.Get<IAssetService>();

    auto found = loaded.find(file.relativePath);
    IAsset* activeAsset = found != loaded.end() ? found->second : nullptr;

    ImGui::PushStyleColor(ImGuiCol_Text, activeAsset ? Palette::Text : Palette::TextMuted);
    const std::string text = std::string(FileIcon(file.path)) + "  " + label;
    if (ImGui::Selectable(text.c_str(), selectedFile_ == file.relativePath, ImGuiSelectableFlags_SpanAllColumns)) {
        selectedFile_ = file.relativePath;
    }
    ImGui::PopStyleColor();
    ItemTooltip(activeAsset ? "Loaded" : "Not loaded - right-click to load");

    if (!ImGui::BeginPopupContextItem("AssetCtx")) return;
    if (!activeAsset) {
        if (ImGui::MenuItem("Load")) {
            std::string ext = file.path.extension().string();
            Path loadPath(file.relativePath);
            if (ext == ".wav")       assetService.LoadAsset<Sound>(loadPath);
            else if (ext == ".xml")  assetService.LoadAsset<Sprite>(loadPath);
            else if (ext == ".lua")  assetService.LoadAsset<Script>(loadPath);
            // .fs only: ShaderAsset treats its path as the fragment shader and
            // picks up the sibling .vs itself, if there is one.
            else if (ext == ".fs" || ext == ".glsl")
                                     assetService.LoadAsset<Shader>(loadPath);
            else                     assetService.LoadAsset<Texture>(loadPath);
        }
    } else {
        if (ImGui::MenuItem(ICON_FA_ROTATE_LEFT "  Reload")) assetService.ReloadAsset(activeAsset);
        if (ImGui::MenuItem("Unload")) activeAsset->Unload();
    }
    ImGui::EndPopup();
}

} // namespace Elysium
