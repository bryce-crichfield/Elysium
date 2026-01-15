#include "Services/AssetService.h"
#include "Services/LogService.h"
#include "Services/LoadingService.h"
#include "Application.h"
#include "raylib.h"
#include "imgui.h"
#include "Common.h"
#include <filesystem>

#include "Sprite.h"

namespace Elysium::Services {

AssetService::AssetService() {
    name_ = "AssetService";
    hasUi_ = true;
}

void AssetService::Initialize() {
    assetsByName_.clear();
    pathToName_.clear();
    LOG_INFO("AssetService", "Initialized");
}

void AssetService::Shutdown() {
    // Unload all assets and track memory deallocations
    for (auto& pair : assetsByName_) {
        Asset& asset = pair.second;
        if (asset.IsLoaded()) {
            asset.Unload();
            // Note: We can't track exact deallocation size for raylib assets
            LOG_INFOF("AssetService", "Unloaded asset: %s", asset.GetName().c_str());
        }
    }

    assetsByName_.clear();
    pathToName_.clear();
    LOG_INFO("AssetService", "Shutdown complete");
}

void AssetService::Update(float deltaTime) {
    Profile;
    // AssetService doesn't need per-frame updates currently
    // This method is here to satisfy the Service interface
}

// ----------------------------------------------------------------------------
// Helper: Convert AssetType enum to string for UI display
// ----------------------------------------------------------------------------
static const char* GetAssetTypeString(AssetType type) {
    switch (type) {
        case AssetType::TEXTURE: return "Texture";
        case AssetType::SOUND:   return "Sound";
        case AssetType::MUSIC:   return "Music";
        case AssetType::FONT:    return "Font";
        case AssetType::MODEL:   return "Model";
        case AssetType::SHADER:  return "Shader";
        case AssetType::SPRITE:  return "Sprite";
        default:                 return "Unknown";
    }
}

// ----------------------------------------------------------------------------
// AssetService::ImGui Implementation
// ----------------------------------------------------------------------------
namespace fs = std::filesystem;
void AssetService::ImGui() {
    Profile;

    // --- 1. Robust Root Path Detection ---
    static fs::path rootPath = "";
    if (rootPath.empty()) {
        // Try current directory
        if (fs::exists("./Assets")) {
            rootPath = fs::canonical("./Assets");
        } 
        // Try up one level (common in build/ bin/ setups)
        else if (fs::exists("../Assets")) {
            rootPath = fs::canonical("../Assets");
        }
        else {
            // Fallback: If we can't find it, we'll show an error in the UI
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "FATAL: Could not locate Assets folder!");
            if (ImGui::Button("Retry Discovery")) rootPath = ""; 
            return;
        }
    }

    static fs::path currentPath = rootPath;
    static std::string selectedFile = "";
    static char assetNameBuffer[128] = "NewAsset";

    if (ImGui::Button("Load New Asset")) {
        ImGui::OpenPopup("AssetFileBrowser");
    }

    // ... [Maintain your Table code here] ...
// --- Asset Table Snippet ---
static ImGuiTableFlags tableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | 
                                    ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY | 
                                    ImGuiTableFlags_SizingStretchProp;

// Use -1 for height to fill remaining available space
if (ImGui::BeginTable("AssetServiceTable", 5, tableFlags, ImVec2(0, -1))) {
    
    // Setup Headers
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 70.0f);
    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Details", ImGuiTableColumnFlags_WidthFixed, 130.0f);
    ImGui::TableSetupColumn("Relative Path", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    for (auto& pair : assetsByName_) {
        const std::string& name = pair.first;
        Asset& asset = pair.second;

        ImGui::TableNextRow();

        // 1. Name
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(name.c_str());

        // 2. Type (using your helper or a switch)
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(GetAssetTypeString(asset.GetType()));

        // 3. Status (Color coded)
        ImGui::TableSetColumnIndex(2);
        if (asset.IsLoaded()) {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Loaded");
        } else if (asset.HasImageData() || asset.HasWaveData()) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Pending");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "Error");
        }

        // 4. Details (Dimensions, IDs, etc.)
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

        // 5. Path
        ImGui::TableSetColumnIndex(4);
        ImGui::TextUnformatted(asset.GetPath().c_str());
    }

    ImGui::EndTable();
}
    // File Chooser Modal
    if (ImGui::BeginPopupModal("AssetFileBrowser", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        
        // --- 2. Navigation & Path Calculation ---
        bool isAtRoot = fs::equivalent(currentPath, rootPath);
        
        ImGui::BeginDisabled(isAtRoot);
        if (ImGui::Button("..")) {
            currentPath = currentPath.parent_path();
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        // Show path relative to the Assets folder for cleanliness
        ImGui::Text("Location: Assets/%s", fs::relative(currentPath, rootPath).string().c_str());
        ImGui::Separator();

        // --- 3. File Listing ---
        ImGui::BeginChild("Files", ImVec2(450, 300), true);
        for (const auto& entry : fs::directory_iterator(currentPath)) {
            const auto& path = entry.path();
            if (entry.is_directory()) {
                if (ImGui::Selectable(("[DIR] " + path.filename().string()).c_str())) {
                    currentPath = path;
                }
            } else {
                std::string fileName = path.filename().string();
                if (ImGui::Selectable(fileName.c_str(), selectedFile == fileName)) {
                    selectedFile = fileName;
                    strncpy(assetNameBuffer, path.stem().string().c_str(), 127);
                }
            }
        }
        ImGui::EndChild();

        // --- 4. Resulting Path (Corrected) ---
        if (!selectedFile.empty()) {
            fs::path fullPath = currentPath / selectedFile;
            
            // Calculate path relative to the Assets folder itself
            // e.g. "Sprites/mushroom/idle.png" instead of "Assets/Sprites/..."
            std::string relativeToAssets = fs::relative(fullPath, rootPath).generic_string();
            
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Path: %s", relativeToAssets.c_str());

            if (ImGui::Button("Load", ImVec2(120, 0))) {
                AssetType type = AssetType::TEXTURE;
                std::string ext = fullPath.extension().string();
                if (ext == ".xml") type = AssetType::SPRITE;
                else if (ext == ".wav") type = AssetType::SOUND;
                
                // Pass the clean relative path. 
                // Note: If your AssetService REQUIRES the "Assets/" prefix, 
                // change the relativeToAssets line back, but check your working directory!
                auto &loadingService = Application::GetInstance().GetService<LoadingService>();
                loadingService.LoadAssets(std::vector<Asset>{Asset(type, assetNameBuffer, relativeToAssets)});
                
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void AssetService::LoadAsset(const Asset& unloadedAsset) {
    const std::string& name = unloadedAsset.GetName();
    const std::string& path = unloadedAsset.GetPath();

    // Check if already loaded by name
    if (IsAssetLoaded(name)) {
        LOG_DEBUGF("AssetService", "Asset already loaded: %s", name.c_str());
        return;
    }

    // Check path deduplication - if this path is already loaded under another name
    auto pathIt = pathToName_.find(path);
    if (pathIt != pathToName_.end()) {
        LOG_DEBUGF("AssetService", "Path %s already loaded as %s, skipping duplicate", path.c_str(), pathIt->second.c_str());
        return;
    }

    // Create a copy of the asset template and attempt to load it
    Asset asset = unloadedAsset;
    LoadAssetByType(asset);

    if (asset.IsLoaded() || asset.HasImageData() || asset.HasWaveData()) {
        // Store the asset (either loaded or with raw data)
        assetsByName_[name] = asset;
        pathToName_[path] = name;


        if (asset.IsLoaded()) {
            LOG_INFOF("AssetService", "Successfully loaded asset: %s -> %s", name.c_str(), path.c_str());
        } else {
            LOG_INFOF("AssetService", "Raw data loaded for asset: %s -> %s (will finalize on main thread)", name.c_str(), path.c_str());
        }
    } else {
        LOG_WARNINGF("AssetService", "Failed to load asset: %s -> %s", name.c_str(), path.c_str());
    }
}

bool AssetService::IsAssetLoaded(const AssetName& name) const {
    auto it = assetsByName_.find(name);
    return it != assetsByName_.end() && it->second.IsLoaded();
}

Asset* AssetService::GetAsset(const AssetName& name) {
    auto it = assetsByName_.find(name);
    if (it != assetsByName_.end() && it->second.IsLoaded()) {
        return &it->second;
    }
    return nullptr;
}

Texture2D AssetService::GetTexture(const AssetName& name) {
    Asset* asset = GetAsset(name);
    if (asset && asset->GetType() == AssetType::TEXTURE) {
        return asset->GetTexture();
    }

    LOG_WARNINGF("AssetService", "Texture not found: %s", name.c_str());
    return Texture2D{0, 0, 0, 0, 0};
}

Sound AssetService::GetSound(const AssetName& name) {
    Asset* asset = GetAsset(name);
    if (asset && asset->GetType() == AssetType::SOUND) {
        return asset->GetSound();
    }

    LOG_WARNINGF("AssetService", "Sound not found: %s", name.c_str());
    return Sound{nullptr, 0, 0, 0, 0};
}

Music AssetService::GetMusic(const AssetName& name) {
    Asset* asset = GetAsset(name);
    if (asset && asset->GetType() == AssetType::MUSIC) {
        return asset->GetMusic();
    }

    LOG_WARNINGF("AssetService", "Music not found: %s", name.c_str());
    return Music{nullptr, 0, 0, 0, 0};
}

Font AssetService::GetFont(const AssetName& name) {
    Asset* asset = GetAsset(name);
    if (asset && asset->GetType() == AssetType::FONT) {
        return asset->GetFont();
    }

    LOG_WARNINGF("AssetService", "Font not found: %s", name.c_str());
    return Font{};
}

Model AssetService::GetModel(const AssetName& name) {
    Asset* asset = GetAsset(name);
    if (asset && asset->GetType() == AssetType::MODEL) {
        return asset->GetModel();
    }

    LOG_WARNINGF("AssetService", "Model not found: %s", name.c_str());
    return Model{};
}

Shader AssetService::GetShader(const AssetName& name) {
    Asset* asset = GetAsset(name);
    if (asset && asset->GetType() == AssetType::SHADER) {
        return asset->GetShader();
    }

    LOG_WARNINGF("AssetService", "Shader not found: %s", name.c_str());
    return Shader{};
}

Sprite AssetService::GetSprite(const AssetName& name) {
    Asset* asset = GetAsset(name);
    if (asset && asset->GetType() == AssetType::SPRITE) {
        return asset->GetSprite();
    }

    LOG_WARNINGF("AssetService", "Sprite not found: %s", name.c_str());
    return Sprite{};
}

void AssetService::LoadAssetByType(Asset& asset) {
    const std::string& path = asset.GetPath();

    LOG_DEBUGF("AssetService", "Loading asset type %d from path %s", (int)asset.GetType(), path.c_str());

    switch (asset.GetType()) {
        case AssetType::TEXTURE: {
            LOG_DEBUGF("AssetService", "Loading texture: %s -> %s", asset.GetName().c_str(), path.c_str());

            // Load image data only (thread-safe)
            Image image = ::LoadImage(path.c_str());
            if (image.data != nullptr) {
                LOG_DEBUGF("AssetService", "Image data loaded: %dx%d, format: %d, mipmaps: %d",
                    image.width, image.height, image.format, image.mipmaps);

                // Store image data for later texture creation on main thread
                asset.SetImageData(image);
            } else {
                LOG_ERRORF("AssetService", "Failed to load image data: %s", path.c_str());
            }
            break;
        }

        case AssetType::SOUND: {
            LOG_DEBUGF("AssetService", "Loading sound: %s -> %s", asset.GetName().c_str(), path.c_str());

            // Try loading sound directly (not thread-safe but for testing)
            Sound sound = ::LoadSound(path.c_str());
            if (sound.frameCount > 0) {
                asset.SetSound(sound);
                LOG_DEBUGF("AssetService", "Sound loaded: %d frames", sound.frameCount);
            } else {
                LOG_WARNINGF("AssetService", "Direct sound load failed, trying wave method: %s", path.c_str());

                // Fallback to wave data approach
                Wave wave = ::LoadWave(path.c_str());
                if (wave.frameCount > 0) {
                    LOG_DEBUGF("AssetService", "Wave data loaded: %d frames, %d Hz, %d channels",
                             wave.frameCount, wave.sampleRate, wave.channels);

                    // Store wave data for later sound creation on main thread
                    asset.SetWaveData(wave);
                } else {
                    LOG_ERRORF("AssetService", "Failed to load sound or wave data: %s", path.c_str());
                }
            }
            break;
        }

        case AssetType::MUSIC: {
            LOG_DEBUGF("AssetService", "Loading music: %s -> %s", asset.GetName().c_str(), path.c_str());
            Music music = ::LoadMusicStream(path.c_str());
            if (music.frameCount > 0) {
                asset.SetMusic(music);
                LOG_DEBUGF("AssetService", "Music loaded: %d frames", music.frameCount);
            } else {
                LOG_ERRORF("AssetService", "Failed to load music: %s", path.c_str());
            }
            break;
        }

        case AssetType::FONT: {
            Font font = ::LoadFont(path.c_str());
            if (font.texture.id != 0) {
                asset.SetFont(font);
                LOG_INFO("AssetService", "Font loaded successfully");
            } else {
                LOG_ERRORF("AssetService", "Failed to load font: %s", path.c_str());
            }
            break;
        }

        case AssetType::MODEL: {
            Model model = ::LoadModel(path.c_str());
            if (model.meshCount > 0) {
                asset.SetModel(model);
                LOG_DEBUGF("AssetService", "Model loaded: %d meshes", model.meshCount);
            } else {
                LOG_ERRORF("AssetService", "Failed to load model: %s", path.c_str());
            }
            break;
        }

        case AssetType::SHADER: {
            // For shaders, we might need vertex and fragment paths
            // For now, load as a fragment shader
            Shader shader = ::LoadShader(nullptr, path.c_str());
            if (shader.id != 0) {
                asset.SetShader(shader);
                LOG_DEBUGF("AssetService", "Shader loaded: ID %d", shader.id);
            } else {
                LOG_ERRORF("AssetService", "Failed to load shader: %s", path.c_str());
            }
            break;
        }

        case AssetType::SPRITE: {
            LOG_DEBUGF("AssetService", "Loading SPRITE asset from %s", path.c_str());
            try {
                Sprite sprite = Sprite::LoadFromXml(path);
                LOG_DEBUGF("AssetService", "Loaded sprite '%s' with %d sheets", sprite.name.c_str(), (int)sprite.sheets.size());
                asset.SetSprite(sprite);
                LOG_DEBUGF("AssetService", "SetSprite completed, asset loaded = %s", asset.IsLoaded() ? "true" : "false");
            } catch (...) {
                LOG_ERRORF("AssetService", "Failed to load sprite: %s", path.c_str());
            }
            break;
        }

        default:
            LOG_WARNINGF("AssetService", "Unknown asset type for: %s", path.c_str());
            break;
    }
}

void AssetService::FinalizeAssets() {
    LOG_INFO("AssetService", "Finalizing assets on main thread");

    for (auto& pair : assetsByName_) {
        Asset& asset = pair.second;

        // Convert image data to texture
        if (asset.HasImageData() && !asset.IsLoaded() && asset.GetType() == AssetType::TEXTURE) {
            Image imageData = asset.GetImageData();
            Texture2D texture = ::LoadTextureFromImage(imageData);

            if (texture.id != 0) {
                asset.SetTexture(texture);
                LOG_DEBUGF("AssetService", "Finalized texture: %s (ID: %d, %dx%d)",
                         asset.GetName().c_str(), texture.id, texture.width, texture.height);
            } else {
                LOG_ERRORF("AssetService", "Failed to finalize texture: %s", asset.GetName().c_str());
            }
        }

        // Convert wave data to sound
        if (asset.HasWaveData() && !asset.IsLoaded() && asset.GetType() == AssetType::SOUND) {
            Wave waveData = asset.GetWaveData();

            LOG_INFOF("AssetService", "Creating sound from wave: %s (%d frames, %d Hz, %d channels)",
                     asset.GetName().c_str(), waveData.frameCount, waveData.sampleRate, waveData.channels);

            // Try converting to mono if stereo
            Wave processedWave = waveData;
            if (waveData.channels == 2) {
                LOG_INFOF("AssetService", "Converting stereo to mono for: %s", asset.GetName().c_str());
                ::WaveFormat(&processedWave, 44100, 16, 1);
            }

            Sound sound = ::LoadSoundFromWave(processedWave);

            if (sound.frameCount > 0) {
                asset.SetSound(sound);
                LOG_INFOF("AssetService", "Finalized sound: %s (%d frames)",
                         asset.GetName().c_str(), sound.frameCount);
            } else {
                LOG_ERRORF("AssetService", "Failed to finalize sound: %s (tried mono conversion)", asset.GetName().c_str());

                // Try the original wave as fallback
                sound = ::LoadSoundFromWave(waveData);
                if (sound.frameCount > 0) {
                    asset.SetSound(sound);
                    LOG_INFOF("AssetService", "Fallback sound creation succeeded: %s", asset.GetName().c_str());
                } else {
                    LOG_ERRORF("AssetService", "Both mono and stereo sound creation failed: %s", asset.GetName().c_str());
                }
            }

            // Clean up processed wave if we modified it
            if (processedWave.data != waveData.data) {
                ::UnloadWave(processedWave);
            }
        }
    }

    LOG_INFO("AssetService", "Asset finalization complete");
}

} // namespace Elysium::Services
