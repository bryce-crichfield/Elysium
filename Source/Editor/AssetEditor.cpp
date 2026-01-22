#include "AssetEditor.h"
#include <filesystem>
#include "Core/Application.h"
#include "Core/Common.h"
#include "Services/AssetService.h"
#include "Services/LoadingService.h"
#include "imgui.h"

namespace Elysium {

namespace fs = std::filesystem;

using namespace Services;

static const char* GetAssetTypeString(AssetType type) {
    switch (type) {
        case AssetType::TEXTURE:
            return "Texture";
        case AssetType::SOUND:
            return "Sound";
        case AssetType::MUSIC:
            return "Music";
        case AssetType::FONT:
            return "Font";
        case AssetType::MODEL:
            return "Model";
        case AssetType::SHADER:
            return "Shader";
        case AssetType::SPRITE:
            return "Sprite";
        case AssetType::SCRIPT:
            return "Script";
        default:
            return "Unknown";
    }
}

AssetEditor::AssetEditor() : Editor("Asset Browser") {}

void AssetEditor::Draw(Application& app) {
    Profile;

    auto& service = app.GetService<AssetService>();

    // Robust Root Path Detection
    if (rootPath_.empty()) {
        if (fs::exists("./Assets")) {
            rootPath_ = fs::canonical("./Assets");
        } else if (fs::exists("../Assets")) {
            rootPath_ = fs::canonical("../Assets");
        }
    }

    ImGui::SetNextWindowSize(ImVec2(600, 450), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(name_.c_str(), &isVisible_, ImGuiWindowFlags_NoCollapse)) {
        if (rootPath_.empty()) {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "FATAL: Could not locate Assets folder!");
            if (ImGui::Button("Retry Discovery"))
                rootPath_ = "";
            ImGui::End();
            return;
        }

        if (currentPath_.empty()) {
            currentPath_ = rootPath_;
        }

        if (ImGui::Button("Load New Asset")) {
            ImGui::OpenPopup("AssetFileBrowser");
        }

        // Asset Table
        static ImGuiTableFlags tableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                            ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY |
                                            ImGuiTableFlags_SizingStretchProp;

        if (ImGui::BeginTable("AssetServiceTable", 5, tableFlags, ImVec2(0, -1))) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Details", ImGuiTableColumnFlags_WidthFixed, 130.0f);
            ImGui::TableSetupColumn("Relative Path", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            for (const auto& [name, asset] : service.GetAllAssets()) {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(name.c_str());

                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(GetAssetTypeString(asset.GetType()));

                ImGui::TableSetColumnIndex(2);
                if (asset.IsLoaded()) {
                    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Loaded");
                } else if (asset.HasImageData() || asset.HasWaveData()) {
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Pending");
                } else {
                    ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "Error");
                }

                ImGui::TableSetColumnIndex(3);
                if (asset.IsLoaded()) {
                    switch (asset.GetType()) {
                        case AssetType::TEXTURE: {
                            Texture2D tex = asset.GetTexture();
                            ImGui::Text("%dx%d", tex.width, tex.height);
                            if (ImGui::IsItemHovered()) {
                                ImGui::BeginTooltip();
                                ImGui::Image((void*)(intptr_t)tex.id, ImVec2(128, 128));
                                ImGui::EndTooltip();
                            }
                            break;
                        }
                        case AssetType::SOUND:
                            ImGui::Text("%u Frames", asset.GetSound().frameCount);
                            break;
                        case AssetType::SPRITE:
                            ImGui::Text("Sprite Loaded");
                            break;
                        case AssetType::SHADER:
                            ImGui::Text("ID: %u", asset.GetShader().id);
                            break;
                        default:
                            ImGui::Text("-");
                            break;
                    }
                } else {
                    ImGui::Text("-");
                }

                ImGui::TableSetColumnIndex(4);
                ImGui::TextUnformatted(asset.GetPath().c_str());
            }

            ImGui::EndTable();
        }

        // File Chooser Modal
        if (ImGui::BeginPopupModal("AssetFileBrowser", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            bool isAtRoot = fs::equivalent(currentPath_, rootPath_);

            ImGui::BeginDisabled(isAtRoot);
            if (ImGui::Button("..")) {
                currentPath_ = currentPath_.parent_path();
            }
            ImGui::EndDisabled();

            ImGui::SameLine();
            ImGui::Text("Location: Assets/%s", fs::relative(currentPath_, rootPath_).string().c_str());
            ImGui::Separator();

            ImGui::BeginChild("Files", ImVec2(450, 300), true);
            for (const auto& entry : fs::directory_iterator(currentPath_)) {
                const auto& path = entry.path();
                if (entry.is_directory()) {
                    if (ImGui::Selectable(("[DIR] " + path.filename().string()).c_str())) {
                        currentPath_ = path;
                    }
                } else {
                    std::string fileName = path.filename().string();
                    if (ImGui::Selectable(fileName.c_str(), selectedFile_ == fileName)) {
                        selectedFile_ = fileName;
                        strncpy(assetNameBuffer_, path.stem().string().c_str(), 127);
                    }
                }
            }
            ImGui::EndChild();

            if (!selectedFile_.empty()) {
                fs::path fullPath = currentPath_ / selectedFile_;
                std::string relativeToAssets = fs::relative(fullPath, rootPath_).generic_string();

                ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Path: %s", relativeToAssets.c_str());

                if (ImGui::Button("Load", ImVec2(120, 0))) {
                    AssetType type = AssetType::TEXTURE;
                    std::string ext = fullPath.extension().string();
                    if (ext == ".xml")
                        type = AssetType::SPRITE;
                    else if (ext == ".wav")
                        type = AssetType::SOUND;
                    else if (ext == ".lua")
                        type = AssetType::SCRIPT;

                    auto& loadingService = app.GetService<LoadingService>();
                    loadingService.LoadAssets(std::vector<Asset>{Asset(type, assetNameBuffer_, relativeToAssets)});

                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }
    ImGui::End();
}

}  // namespace Elysium
