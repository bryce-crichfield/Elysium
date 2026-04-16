#include "AssetEditor.h"
#include "Core/Application.h"
#include "Core/Path.h"
#include "Services/AssetService.h"
#include "imgui.h"

namespace Elysium {

namespace fs = std::filesystem;
using namespace Services;

AssetEditor::AssetEditor() : Editor("Asset Browser") {}

void AssetEditor::RefreshDiskCache() {
    directoryCache_.clear();
    for (const auto& entry : fs::recursive_directory_iterator(rootPath_)) {
        std::string parentPath = entry.path().parent_path().generic_string();
        
        DiskFile file;
        file.path = entry.path();
        file.relativePath = fs::relative(entry.path(), rootPath_).generic_string();
        file.isDirectory = entry.is_directory();
        
        directoryCache_[parentPath].push_back(file);
    }
}

void AssetEditor::Draw(Application& app) {
    auto& assetService = app.GetService<AssetService>();

    // Discovery & Polling
    if (rootPath_.empty()) {
        if (fs::exists("./Assets")) rootPath_ = fs::canonical("./Assets");
        else if (fs::exists("../Assets")) rootPath_ = fs::canonical("../Assets");
    }

    if (rootPath_.empty()) return;

    if (app.GetTime() - lastRefreshTime_ > refreshInterval_) {
        RefreshDiskCache();
        lastRefreshTime_ = app.GetTime();
    }

    if (ImGui::Begin(name_.c_str())) {
        RenderTreeRecursive(rootPath_, app);
    }
    ImGui::End();
}

void AssetEditor::RenderTreeRecursive(const fs::path& currentPath, Application& app) {
    auto& assetService = app.GetService<AssetService>();
    
    std::string pathKey = currentPath.generic_string();
    if (directoryCache_.find(pathKey) == directoryCache_.end()) return;

    for (const auto& file : directoryCache_[pathKey]) {
        // --- FIX: PUSH UNIQUE ID ---
        // This prevents the "id != 0" assert by ensuring every item has a unique path hash
        ImGui::PushID(file.relativePath.c_str());

        std::string name = file.path.filename().string();
        
        if (file.isDirectory) {
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth;
            if (ImGui::TreeNodeEx(name.c_str(), flags)) {
                RenderTreeRecursive(file.path, app);
                ImGui::TreePop();
            }
        } else {
            // Optimization: AssetService has pathToName_, use that if you can
            // otherwise we'll stick to the scan for now.
            const Asset* activeAsset = nullptr;
            for (const auto& [assetName, asset] : assetService.GetAllAssets()) {
                // Compare relative paths (GetPath returns full path, use GetRelativePath instead)
                if (asset.GetPath().GetRelativePath() == file.relativePath) {
                    activeAsset = &asset;
                    break;
                }
            }
            
            bool isLoaded = (activeAsset != nullptr && activeAsset->IsLoaded());
            
            // Apply status coloring
            if (isLoaded) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.2f, 1.0f)); 
            else ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));

            bool isSelected = (selectedFile_ == file.relativePath);
            if (ImGui::Selectable(name.c_str(), isSelected)) {
                selectedFile_ = file.relativePath;
            }

            ImGui::PopStyleColor();

            // Right Click Context Menu
            if (ImGui::BeginPopupContextItem("AssetCtx")) {
                if (!isLoaded) {
                    if (ImGui::MenuItem("Load into Elysium")) {
                        AssetType type = AssetType::TEXTURE;
                        std::string ext = file.path.extension().string();
                        if (ext == ".wav") type = AssetType::SOUND;
                        else if (ext == ".xml") {
                            // if path contains "Sprites" load as SPRITE, if it contains "Tiles" load as TILE, "Characters" load as CHARACTER, otherwise default to SCRIPT
                            if (file.relativePath.find("Sprites/") == 0) type = AssetType::SPRITE;
                            else if (file.relativePath.find("Tiles/") == 0) type = AssetType::TILE;
                            else if (file.relativePath.find("Characters/") == 0) type = AssetType::CHARACTER;
                            else type = AssetType::SCRIPT;
                        }
                        else if (ext == ".lua") type = AssetType::SCRIPT;

                        assetService.LoadAsset(type, Path(file.relativePath));
                    }
                } else {
                    if (ImGui::MenuItem("Reload Asset")) {
                        assetService.ReloadAsset(activeAsset->GetType(), activeAsset->GetPath());
                    }
                    if (ImGui::MenuItem("Unload Asset")) {
                        assetService.GetAsset(activeAsset->GetPath())->Unload();
                    }
                }
                ImGui::EndPopup();
            }
        }

        // --- FIX: POP UNIQUE ID ---
        ImGui::PopID();
    }
}

} // namespace Elysium