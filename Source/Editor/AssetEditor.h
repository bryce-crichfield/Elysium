#pragma once

#include <filesystem>
#include <string>
#include <map>
#include <vector>
#include "Core/Editor.h"

namespace Elysium {

struct DiskFile {
    std::filesystem::path path;
    std::string relativePath;
    bool isDirectory;
    bool isLoaded; // Synced from AssetService
};

class AssetEditor : public Editor {
public:
    AssetEditor();
    void Draw(Application& app) override;

private:
    void RefreshDiskCache();
    void RenderTreeRecursive(const std::filesystem::path& currentPath, Application& app);

    std::filesystem::path rootPath_;
    
    // Polling state
    double lastRefreshTime_ = 0.0;
    const double refreshInterval_ = 1.0; 

    // Caching the directory structure to avoid heavy IO every frame
    std::string selectedFile_;
    std::map<std::string, std::vector<DiskFile>> directoryCache_;
};

} // namespace Elysium