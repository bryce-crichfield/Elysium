#include "Services/AssetService.h"
#include <filesystem>
#include "Core/Application.h"
#include "Core/Common.h"
#include "Services/LoadingService.h"
#include "Services/LogService.h"
#include "imgui.h"
#include "raylib.h"

#include "Core/Sprite.h"

namespace Elysium::Services {

AssetService::AssetService() {
    name_ = "AssetService";
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

void AssetService::ReloadAsset(const AssetName& name) {
    auto it = assetsByName_.find(name);
    if (it == assetsByName_.end()) {
        LOG_WARNINGF("AssetService", "Cannot reload asset '%s', not found", name.c_str());
        return;
    }

    Asset& asset = it->second;
    if (asset.IsLoaded()) {
        asset.Unload();
    }

    LOG_INFOF("AssetService", "Reloading asset: %s", name.c_str());
    LoadAssetByType(asset);

    if (asset.IsLoaded() || asset.HasImageData() || asset.HasWaveData()) {
        LOG_INFOF("AssetService", "Successfully reloaded asset: %s", name.c_str());
    } else {
        LOG_ERRORF("AssetService", "Failed to reload asset: %s", name.c_str());
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

Script AssetService::GetScript(const AssetName& name) {
    Asset* asset = GetAsset(name);
    if (asset && asset->GetType() == AssetType::SCRIPT) {
        return asset->GetScript();
    }

    LOG_WARNINGF("AssetService", "Script not found: %s", name.c_str());
    return "";
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

        case AssetType::SCRIPT: {
            LOG_DEBUGF("AssetService", "Loading SCRIPT asset from %s", path.c_str());
            char* text = ::LoadFileText(path.c_str());
            if (text) {
                asset.SetScript(std::string(text));
                ::UnloadFileText(text);
                LOG_DEBUGF("AssetService", "Script loaded: %s", asset.GetName().c_str());
            } else {
                LOG_ERRORF("AssetService", "Failed to load script: %s", path.c_str());
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

}  // namespace Elysium::Services
