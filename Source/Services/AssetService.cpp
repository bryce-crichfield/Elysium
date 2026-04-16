#include "Services/AssetService.h"
#include <filesystem>
#include "Core/Application.h"
#include "Core/Common.h"
#include "Services/LogService.h"
#include "Services/TaskService.h"
#include "imgui.h"
#include "raylib.h"

#include "Core/Sprite.h"
#include "Core/Tile.h"

namespace Elysium::Services {

AssetService::AssetService() {
    name_ = "AssetService";
}

void AssetService::Initialize() {
    assetsByPath_.clear();
    LOG_INFO("AssetService", "Initialized");
}

void AssetService::Shutdown() {
    for (auto& pair : assetsByPath_) {
        Asset& asset = pair.second;
        if (asset.IsLoaded()) {
            asset.Unload();
            LOG_INFOF("AssetService", "Unloaded asset: %s", asset.GetPath().c_str());
        }
    }

    assetsByPath_.clear();
    pendingFutures_.clear();
    LOG_INFO("AssetService", "Shutdown complete");
}

void AssetService::Update(float deltaTime) {
    Profile;

    // Check if all pending futures have resolved — if so, finalize
    if (needsFinalization_) {
        bool allDone = true;
        for (auto& f : pendingFutures_) {
            if (!f.IsReady()) {
                allDone = false;
                break;
            }
        }
        if (allDone) {
            FinalizeAssets();
            pendingFutures_.clear();
            needsFinalization_ = false;
        }
    }
}

Future<Asset> AssetService::LoadAsset(AssetType type, Path path) {
    // Check map first — covers both fully-loaded and in-flight (placeholder) cases.
    auto existing = assetsByPath_.find(path);
    if (existing != assetsByPath_.end()) {
        Future<Asset> future;
        if (existing->second.IsLoaded()) {
            LOG_DEBUGF("AssetService", "Asset already loaded: %s", path.c_str());
            future.Resolve(existing->second);
        }
        // else: raw data is pending finalization — do nothing, caller will get the
        // texture on a subsequent frame once FinalizeAssets() runs.
        return future;
    }

    // Insert a placeholder immediately so no other caller re-queues the same path
    // while the background task is in flight.
    assetsByPath_[path] = Asset(type, path);

    auto& taskService = Application::GetInstance().GetService<TaskService>();

    Future<Asset> future = taskService.Submit<Asset>(
        std::function<Asset()>([type, path]() -> Asset {
            return LoadAssetData(type, path);
        }));

    // Capture 'this' for cache insertion — runs on main thread via TaskService::Update()
    future.Then([this, path](const Asset& asset) {
        if (asset.IsLoaded() || asset.HasImageData() || asset.HasWaveData()) {
            assetsByPath_[path] = asset;

            if (asset.IsLoaded()) {
                LOG_INFOF("AssetService", "Loaded asset: %s", path.c_str());
            } else {
                LOG_INFOF("AssetService", "Raw data loaded: %s (needs finalization)", path.c_str());
                needsFinalization_ = true;
            }

            // For sprites, kick off sheet texture loads on the main thread
            if (asset.GetType() == AssetType::SPRITE) {
                Sprite sprite = asset.GetSprite();
                for (auto& [sheetName, sheet] : sprite.sheets) {
                    std::string relativePath = "Sprites/" + sheet.path;
                    Path sheetPath = Path(relativePath);
                    if (!IsAssetLoaded(sheetPath)) {
                        LOG_DEBUGF("AssetService", "Loading sheet texture: %s", sheetPath.c_str());
                        LoadAsset(AssetType::TEXTURE, sheetPath);
                    }
                }
            }

            // For tiles, kick off the sheet texture load on the main thread
            if (asset.GetType() == AssetType::TILE) {
                Tile tile = asset.GetTile();
                if (!tile.sheet.path.empty()) {
                    std::string relativePath = "Tiles/" + tile.sheet.path;
                    Path sheetPath = Path(relativePath);
                    if (!IsAssetLoaded(sheetPath)) {
                        LOG_DEBUGF("AssetService", "Loading tile sheet texture: %s", sheetPath.c_str());
                        LoadAsset(AssetType::TEXTURE, sheetPath);
                    }
                }
            }
        } else {
            LOG_WARNINGF("AssetService", "Failed to load asset: %s", path.c_str());
        }
    });

    pendingFutures_.push_back(future);
    needsFinalization_ = true;

    return future;
}

Future<Asset> AssetService::ReloadAsset(AssetType type, Path path) {
    // Unload existing asset on main thread before submitting reload
    auto it = assetsByPath_.find(path);
    if (it != assetsByPath_.end()) {
        if (it->second.IsLoaded()) {
            it->second.Unload();
        }
        assetsByPath_.erase(it);
    }

    LOG_INFOF("AssetService", "Reloading asset: %s", path.c_str());
    return LoadAsset(type, path);
}

bool AssetService::IsAssetLoaded(Path path) const {
    auto it = assetsByPath_.find(path);
    return it != assetsByPath_.end() && it->second.IsLoaded();
}

Asset* AssetService::GetAsset(Path path) {
    auto it = assetsByPath_.find(path);
    if (it != assetsByPath_.end() && it->second.IsLoaded()) {
        return &it->second;
    }
    return nullptr;
}

Texture2D AssetService::GetTexture(Path path) {
    Asset* asset = GetAsset(path);
    if (asset && asset->GetType() == AssetType::TEXTURE) {
        return asset->GetTexture();
    }

    return Texture2D{0, 0, 0, 0, 0};
}

Sound AssetService::GetSound(Path path) {
    Asset* asset = GetAsset(path);
    if (asset && asset->GetType() == AssetType::SOUND) {
        return asset->GetSound();
    }

    return Sound{nullptr, 0, 0, 0, 0};
}

Music AssetService::GetMusic(Path path) {
    Asset* asset = GetAsset(path);
    if (asset && asset->GetType() == AssetType::MUSIC) {
        return asset->GetMusic();
    }

    return Music{nullptr, 0, 0, 0, 0};
}

Font AssetService::GetFont(Path path) {
    Asset* asset = GetAsset(path);
    if (asset && asset->GetType() == AssetType::FONT) {
        return asset->GetFont();
    }

    return Font{};
}

Model AssetService::GetModel(Path path) {
    Asset* asset = GetAsset(path);
    if (asset && asset->GetType() == AssetType::MODEL) {
        return asset->GetModel();
    }

    return Model{};
}

Shader AssetService::GetShader(Path path) {
    Asset* asset = GetAsset(path);
    if (asset && asset->GetType() == AssetType::SHADER) {
        return asset->GetShader();
    }

    return Shader{};
}

Sprite AssetService::GetSprite(Path path) {
    Asset* asset = GetAsset(path);
    if (asset && asset->GetType() == AssetType::SPRITE) {
        return asset->GetSprite();
    }

    return Sprite{};
}

Script AssetService::GetScript(Path path) {
    Asset* asset = GetAsset(path);
    if (asset && asset->GetType() == AssetType::SCRIPT) {
        return asset->GetScript();
    }

    return Script{};
}

Tile AssetService::GetTile(Path path) {
    Asset* asset = GetAsset(path);
    if (asset && asset->GetType() == AssetType::TILE) {
        return asset->GetTile();
    }

    return Tile{};
}

// Thread-safe I/O — does NOT touch assetsByPath_
Asset AssetService::LoadAssetData(AssetType type, Path path) {
    Asset asset(type, path);

    LOG_DEBUGF("AssetService", "Loading asset type %d from path %s", (int)type, path.c_str());

    switch (type) {
        case AssetType::TEXTURE: {
            Image image = ::LoadImage(path.c_str());
            if (image.data != nullptr) {
                LOG_DEBUGF("AssetService", "Image data loaded: %dx%d, format: %d, mipmaps: %d",
                           image.width, image.height, image.format, image.mipmaps);
                asset.SetImageData(image);
            } else {
                LOG_ERRORF("AssetService", "Failed to load image data: %s", path.c_str());
            }
            break;
        }

        case AssetType::SOUND: {
            // Load wave data (thread-safe) for later conversion on main thread
            Wave wave = ::LoadWave(path.c_str());
            if (wave.frameCount > 0) {
                LOG_DEBUGF("AssetService", "Wave data loaded: %d frames, %d Hz, %d channels",
                           wave.frameCount, wave.sampleRate, wave.channels);
                asset.SetWaveData(wave);
            } else {
                LOG_ERRORF("AssetService", "Failed to load wave data: %s", path.c_str());
            }
            break;
        }

        case AssetType::MUSIC: {
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
                Sprite sprite = Sprite::LoadFromXml(path.GetFullPath());
                LOG_DEBUGF("AssetService", "Loaded sprite '%s' with %d sheets",
                           sprite.name.c_str(), (int)sprite.sheets.size());
                asset.SetSprite(sprite);
            } catch (...) {
                LOG_ERRORF("AssetService", "Failed to load sprite: %s", path.c_str());
            }
            break;
        }

        case AssetType::SCRIPT: {
            LOG_DEBUGF("AssetService", "Loading SCRIPT asset from %s", path.c_str());
            char* text = ::LoadFileText(path.c_str());
            if (text) {
                Script script = {std::string(text), path};
                asset.SetScript(script);
                ::UnloadFileText(text);
                LOG_DEBUGF("AssetService", "Script loaded: %s", path.c_str());
            } else {
                LOG_ERRORF("AssetService", "Failed to load script: %s", path.c_str());
            }
            break;
        }

        case AssetType::TILE: {
            LOG_DEBUGF("AssetService", "Loading TILE asset from %s", path.c_str());
            try {
                Tile tile = Tile::LoadFromXml(path.GetFullPath());
                LOG_DEBUGF("AssetService", "Loaded tile '%s' with %d variants",
                           tile.name.c_str(), (int)tile.variants.size());
                asset.SetTile(tile);
            } catch (...) {
                LOG_ERRORF("AssetService", "Failed to load tile: %s", path.c_str());
            }
            break;
        }

        default:
            LOG_WARNINGF("AssetService", "Unknown asset type for: %s", path.c_str());
            break;
    }

    return asset;
}

void AssetService::FinalizeAssets() {
    LOG_INFO("AssetService", "Finalizing assets on main thread");

    for (auto& pair : assetsByPath_) {
        Asset& asset = pair.second;

        // Convert image data to texture
        if (asset.HasImageData() && !asset.IsLoaded() && asset.GetType() == AssetType::TEXTURE) {
            Image imageData = asset.GetImageData();
            Texture2D texture = ::LoadTextureFromImage(imageData);

            if (texture.id != 0) {
                asset.SetTexture(texture);
                LOG_DEBUGF("AssetService", "Finalized texture: %s (ID: %d, %dx%d)",
                           asset.GetPath().c_str(), texture.id, texture.width, texture.height);
            } else {
                LOG_ERRORF("AssetService", "Failed to finalize texture: %s", asset.GetPath().c_str());
            }
        }

        // Convert wave data to sound
        if (asset.HasWaveData() && !asset.IsLoaded() && asset.GetType() == AssetType::SOUND) {
            Wave waveData = asset.GetWaveData();

            LOG_INFOF("AssetService", "Creating sound from wave: %s (%d frames, %d Hz, %d channels)",
                      asset.GetPath().c_str(), waveData.frameCount, waveData.sampleRate, waveData.channels);

            Wave processedWave = waveData;
            if (waveData.channels == 2) {
                LOG_INFOF("AssetService", "Converting stereo to mono for: %s", asset.GetPath().c_str());
                ::WaveFormat(&processedWave, 44100, 16, 1);
            }

            Sound sound = ::LoadSoundFromWave(processedWave);

            if (sound.frameCount > 0) {
                asset.SetSound(sound);
                LOG_INFOF("AssetService", "Finalized sound: %s (%d frames)",
                          asset.GetPath().c_str(), sound.frameCount);
            } else {
                LOG_ERRORF("AssetService", "Failed to finalize sound: %s (tried mono conversion)", asset.GetPath().c_str());

                sound = ::LoadSoundFromWave(waveData);
                if (sound.frameCount > 0) {
                    asset.SetSound(sound);
                    LOG_INFOF("AssetService", "Fallback sound creation succeeded: %s", asset.GetPath().c_str());
                } else {
                    LOG_ERRORF("AssetService", "Both mono and stereo sound creation failed: %s", asset.GetPath().c_str());
                }
            }

            if (processedWave.data != waveData.data) {
                ::UnloadWave(processedWave);
            }
        }
    }

    LOG_INFO("AssetService", "Asset finalization complete");
}

}  // namespace Elysium::Services
