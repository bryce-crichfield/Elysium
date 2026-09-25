#pragma once

#include <filesystem>
#include <string>
#include <map>
#include <unordered_map>
#include <vector>
#include "Core/Editor.h"
#include "Core/Future.h"

namespace Elysium {

class IAsset;

struct DiskFile {
    std::filesystem::path path;
    std::string relativePath;
    bool isDirectory;
};

using DiskCache = std::map<std::string, std::vector<DiskFile>>;

class AssetEditor : public Editor {
public:
    static constexpr const char* Title = "Assets";

    explicit AssetEditor(ServiceLocator& services);
    void Draw() override;

private:
    // Loaded assets by project-relative path, rebuilt each frame for the tree to look up.
    using LoadedAssets = std::unordered_map<std::string, IAsset*>;

    void RefreshDiskCacheIfDue();
    void DrawTree(const std::filesystem::path& currentPath, const LoadedAssets& loaded);
    // Every file under the root whose relative path matches the search, as a flat list.
    void DrawSearchResults(const LoadedAssets& loaded);
    void DrawFile(const DiskFile& file, const LoadedAssets& loaded, const std::string& label);

    std::filesystem::path rootPath_;
    char searchBuffer_[128] = "";

    // Polling state
    double lastRefreshTime_ = 0.0;
    const double refreshInterval_ = 1.0;

    // Caching the directory structure to avoid heavy IO every frame.
    // The scan itself runs on a background thread (TaskService) so a slow disk
    // (e.g. a OneDrive-synced folder) doesn't stall the main/render thread.
    std::string selectedFile_;
    DiskCache directoryCache_;
    bool refreshInFlight_ = false;
};

} // namespace Elysium
