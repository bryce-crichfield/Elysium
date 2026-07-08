#pragma once

#include <filesystem>
#include <string>
#include <map>
#include <vector>
#include "Core/Editor.h"
#include "Core/Future.h"

namespace Elysium {

struct DiskFile {
    std::filesystem::path path;
    std::string relativePath;
    bool isDirectory;
    bool isLoaded; // Synced from AssetService
};

using DiskCache = std::map<std::string, std::vector<DiskFile>>;

class AssetEditor : public Editor {
public:
    AssetEditor();
    void Draw(Application& app) override;

private:
    void RenderTreeRecursive(const std::filesystem::path& currentPath, Application& app);

    std::filesystem::path rootPath_;

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