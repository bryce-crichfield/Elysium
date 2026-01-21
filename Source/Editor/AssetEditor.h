#pragma once

#include <filesystem>
#include <string>
#include "Core/Editor.h"

namespace Elysium::Services {
class AssetService;
}

namespace Elysium {

class AssetEditor : public Editor {
   public:
    AssetEditor();

    void Draw(Application& app) override;

   private:
    // File browser state
    std::filesystem::path rootPath_;
    std::filesystem::path currentPath_;
    std::string selectedFile_;
    char assetNameBuffer_[128] = "NewAsset";
};

}  // namespace Elysium
