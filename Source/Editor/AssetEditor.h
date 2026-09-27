#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include "Core/AssetKind.h"
#include "Core/Editor.h"
#include "Editor/AssetFileDialog.h"
#include "Core/Future.h"

namespace Elysium {

class IAsset;

struct DiskFile {
    std::filesystem::path path;
    std::string relativePath;  // to the project root, forward slashes
    AssetKind kind = AssetKind::Folder;
    uintmax_t size = 0;
};

// Files by parent folder (relative to the project root, "" for the root itself).
using DiskCache = std::map<std::string, std::vector<DiskFile>>;

class AssetEditor : public Editor {
public:
    static constexpr const char* Title = "Assets";

    explicit AssetEditor(ServiceLocator& services);
    void Draw() override;

private:
    // Loaded assets by project-relative path, rebuilt each frame for the views to look up.
    using LoadedAssets = std::unordered_map<std::string, IAsset*>;

    void RefreshDiskCacheIfDue();
    void DrawNavBar();
    void DrawChips();
    // The files on show: the current folder, or with a search, every match in the project.
    std::vector<const DiskFile*> VisibleFiles() const;
    void DrawGrid(const std::vector<const DiskFile*>& files, const LoadedAssets& loaded);
    void DrawList(const std::vector<const DiskFile*>& files, const LoadedAssets& loaded);
    // Selection, double-click to open, and the context menu, for the item just drawn.
    void HandleItem(const DiskFile& file, IAsset* asset, bool clicked, bool doubleClicked);
    void Open(const DiskFile& file);
    void NavigateTo(const std::string& folder);

    std::filesystem::path rootPath_;
    char searchBuffer_[128] = "";
    bool gridView_ = true;

    AssetFileDialog createDialog_;
    std::string currentFolder_;  // relative to the project root
    std::vector<std::string> back_, forward_;
    std::string selectedFile_;

    // Polling state
    double lastRefreshTime_ = 0.0;
    const double refreshInterval_ = 1.0;

    // Caching the directory structure to avoid heavy IO every frame.
    // The scan itself runs on a background thread (TaskService) so a slow disk
    // (e.g. a OneDrive-synced folder) doesn't stall the main/render thread.
    DiskCache directoryCache_;
    bool refreshInFlight_ = false;
};

} // namespace Elysium
