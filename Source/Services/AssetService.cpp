#include "Services/AssetService.h"
#include "Services/LogService.h"
#include "raylib.h"

namespace Elysium::Services {

void AssetService::Initialize() {
    assetsByName_.clear();
    pathToName_.clear();
    LOG_SERVICE_INFO("ASSET_SERVICE", "Initialized");
}

void AssetService::Shutdown() {
    // Unload all assets and track memory deallocations
    for (auto& pair : assetsByName_) {
        Asset& asset = pair.second;
        if (asset.IsLoaded()) {
            asset.Unload();
            // Note: We can't track exact deallocation size for raylib assets
            LOG_SERVICE_INFOF("ASSET_SERVICE", "Unloaded asset: %s", asset.GetName().c_str());
        }
    }
    
    assetsByName_.clear();
    pathToName_.clear();
    LOG_SERVICE_INFO("ASSET_SERVICE", "Shutdown complete");
}

void AssetService::LoadAsset(const Asset& assetTemplate) {
    const std::string& name = assetTemplate.GetName();
    const std::string& path = assetTemplate.GetPath();
    
    // Check if already loaded by name
    if (IsAssetLoaded(name)) {
        LOG_SERVICE_INFOF("ASSET_SERVICE", "Asset already loaded: %s", name.c_str());
        return;
    }
    
    // Check path deduplication - if this path is already loaded under another name
    auto pathIt = pathToName_.find(path);
    if (pathIt != pathToName_.end()) {
        LOG_SERVICE_INFOF("ASSET_SERVICE", "Path %s already loaded as %s, skipping duplicate", path.c_str(), pathIt->second.c_str());
        return;
    }
    
    // Create a copy of the asset template and attempt to load it
    Asset asset = assetTemplate;
    LoadAssetByType(asset);
    
    if (asset.IsLoaded() || asset.HasImageData() || asset.HasWaveData()) {
        // Store the asset (either loaded or with raw data)
        assetsByName_[name] = asset;
        pathToName_[path] = name;
        
        // Track memory allocation (we don't know exact size for raylib assets)
        MemoryTracker::GetInstance().RecordAllocation(1); // Record that an allocation occurred
        
        if (asset.IsLoaded()) {
            LOG_SERVICE_INFOF("ASSET_SERVICE", "Successfully loaded asset: %s -> %s", name.c_str(), path.c_str());
        } else {
            LOG_SERVICE_INFOF("ASSET_SERVICE", "Raw data loaded for asset: %s -> %s (will finalize on main thread)", name.c_str(), path.c_str());
        }
    } else {
        LOG_SERVICE_WARNINGF("ASSET_SERVICE", "Failed to load asset: %s -> %s", name.c_str(), path.c_str());
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
    
    LOG_SERVICE_WARNINGF("ASSET_SERVICE", "Texture not found: %s", name.c_str());
    return Texture2D{0, 0, 0, 0, 0};
}

Sound AssetService::GetSound(const AssetName& name) {
    Asset* asset = GetAsset(name);
    if (asset && asset->GetType() == AssetType::SOUND) {
        return asset->GetSound();
    }
    
    LOG_SERVICE_WARNINGF("ASSET_SERVICE", "Sound not found: %s", name.c_str());
    return Sound{nullptr, 0, 0, 0, 0};
}

Music AssetService::GetMusic(const AssetName& name) {
    Asset* asset = GetAsset(name);
    if (asset && asset->GetType() == AssetType::MUSIC) {
        return asset->GetMusic();
    }
    
    LOG_SERVICE_WARNINGF("ASSET_SERVICE", "Music not found: %s", name.c_str());
    return Music{nullptr, 0, 0, 0, 0};
}

Font AssetService::GetFont(const AssetName& name) {
    Asset* asset = GetAsset(name);
    if (asset && asset->GetType() == AssetType::FONT) {
        return asset->GetFont();
    }
    
    LOG_SERVICE_WARNINGF("ASSET_SERVICE", "Font not found: %s", name.c_str());
    return Font{};
}

Model AssetService::GetModel(const AssetName& name) {
    Asset* asset = GetAsset(name);
    if (asset && asset->GetType() == AssetType::MODEL) {
        return asset->GetModel();
    }
    
    LOG_SERVICE_WARNINGF("ASSET_SERVICE", "Model not found: %s", name.c_str());
    return Model{};
}

Shader AssetService::GetShader(const AssetName& name) {
    Asset* asset = GetAsset(name);
    if (asset && asset->GetType() == AssetType::SHADER) {
        return asset->GetShader();
    }
    
    LOG_SERVICE_WARNINGF("ASSET_SERVICE", "Shader not found: %s", name.c_str());
    return Shader{};
}

void AssetService::LoadAssetByType(Asset& asset) {
    const std::string& path = asset.GetPath();
    
    switch (asset.GetType()) {
        case AssetType::TEXTURE: {
            LOG_SERVICE_INFOF("ASSET_SERVICE", "Loading texture: %s -> %s", asset.GetName().c_str(), path.c_str());
            
            // Load image data only (thread-safe)
            Image image = ::LoadImage(path.c_str());
            if (image.data != nullptr) {
                LOG_SERVICE_INFOF("ASSET_SERVICE", "Image data loaded: %dx%d, format: %d, mipmaps: %d", 
                    image.width, image.height, image.format, image.mipmaps);
                
                // Store image data for later texture creation on main thread
                asset.SetImageData(image);
            } else {
                LOG_SERVICE_ERRORF("ASSET_SERVICE", "Failed to load image data: %s", path.c_str());
            }
            break;
        }
        
        case AssetType::SOUND: {
            LOG_SERVICE_INFOF("ASSET_SERVICE", "Loading sound: %s -> %s", asset.GetName().c_str(), path.c_str());
            
            // Try loading sound directly (not thread-safe but for testing)
            Sound sound = ::LoadSound(path.c_str());
            if (sound.frameCount > 0) {
                asset.SetSound(sound);
                LOG_SERVICE_INFOF("ASSET_SERVICE", "Sound loaded: %d frames", sound.frameCount);
            } else {
                LOG_SERVICE_WARNINGF("ASSET_SERVICE", "Direct sound load failed, trying wave method: %s", path.c_str());
                
                // Fallback to wave data approach
                Wave wave = ::LoadWave(path.c_str());
                if (wave.frameCount > 0) {
                    LOG_SERVICE_INFOF("ASSET_SERVICE", "Wave data loaded: %d frames, %d Hz, %d channels", 
                             wave.frameCount, wave.sampleRate, wave.channels);
                    
                    // Store wave data for later sound creation on main thread
                    asset.SetWaveData(wave);
                } else {
                    LOG_SERVICE_ERRORF("ASSET_SERVICE", "Failed to load sound or wave data: %s", path.c_str());
                }
            }
            break;
        }
        
        case AssetType::MUSIC: {
            LOG_SERVICE_INFOF("ASSET_SERVICE", "Loading music: %s -> %s", asset.GetName().c_str(), path.c_str());
            Music music = ::LoadMusicStream(path.c_str());
            if (music.frameCount > 0) {
                asset.SetMusic(music);
                LOG_SERVICE_INFOF("ASSET_SERVICE", "Music loaded: %d frames", music.frameCount);
            } else {
                LOG_SERVICE_ERRORF("ASSET_SERVICE", "Failed to load music: %s", path.c_str());
            }
            break;
        }
        
        case AssetType::FONT: {
            Font font = ::LoadFont(path.c_str());
            if (font.texture.id != 0) {
                asset.SetFont(font);
                LOG_SERVICE_INFO("ASSET_SERVICE", "Font loaded successfully");
            } else {
                LOG_SERVICE_ERRORF("ASSET_SERVICE", "Failed to load font: %s", path.c_str());
            }
            break;
        }
        
        case AssetType::MODEL: {
            Model model = ::LoadModel(path.c_str());
            if (model.meshCount > 0) {
                asset.SetModel(model);
                LOG_SERVICE_INFOF("ASSET_SERVICE", "Model loaded: %d meshes", model.meshCount);
            } else {
                LOG_SERVICE_ERRORF("ASSET_SERVICE", "Failed to load model: %s", path.c_str());
            }
            break;
        }
        
        case AssetType::SHADER: {
            // For shaders, we might need vertex and fragment paths
            // For now, load as a fragment shader
            Shader shader = ::LoadShader(nullptr, path.c_str());
            if (shader.id != 0) {
                asset.SetShader(shader);
                LOG_SERVICE_INFOF("ASSET_SERVICE", "Shader loaded: ID %d", shader.id);
            } else {
                LOG_SERVICE_ERRORF("ASSET_SERVICE", "Failed to load shader: %s", path.c_str());
            }
            break;
        }
        
        default:
            LOG_SERVICE_WARNINGF("ASSET_SERVICE", "Unknown asset type for: %s", path.c_str());
            break;
    }
}

void AssetService::FinalizeAssets() {
    LOG_SECTION_START("ASSET FINALIZATION");
    LOG_SERVICE_INFO("ASSET_SERVICE", "Finalizing assets on main thread");
    
    for (auto& pair : assetsByName_) {
        Asset& asset = pair.second;
        
        // Convert image data to texture
        if (asset.HasImageData() && !asset.IsLoaded() && asset.GetType() == AssetType::TEXTURE) {
            Image imageData = asset.GetImageData();
            Texture2D texture = ::LoadTextureFromImage(imageData);
            
            if (texture.id != 0) {
                asset.SetTexture(texture);
                LOG_SERVICE_INFOF("ASSET_SERVICE", "Finalized texture: %s (ID: %d, %dx%d)", 
                         asset.GetName().c_str(), texture.id, texture.width, texture.height);
            } else {
                LOG_SERVICE_ERRORF("ASSET_SERVICE", "Failed to finalize texture: %s", asset.GetName().c_str());
            }
        }
        
        // Convert wave data to sound
        if (asset.HasWaveData() && !asset.IsLoaded() && asset.GetType() == AssetType::SOUND) {
            Wave waveData = asset.GetWaveData();
            
            LOG_SERVICE_INFOF("ASSET_SERVICE", "Creating sound from wave: %s (%d frames, %d Hz, %d channels)", 
                     asset.GetName().c_str(), waveData.frameCount, waveData.sampleRate, waveData.channels);
            
            // Try converting to mono if stereo
            Wave processedWave = waveData;
            if (waveData.channels == 2) {
                LOG_SERVICE_INFOF("ASSET_SERVICE", "Converting stereo to mono for: %s", asset.GetName().c_str());
                ::WaveFormat(&processedWave, 44100, 16, 1);
            }
            
            Sound sound = ::LoadSoundFromWave(processedWave);
            
            if (sound.frameCount > 0) {
                asset.SetSound(sound);
                LOG_SERVICE_INFOF("ASSET_SERVICE", "Finalized sound: %s (%d frames)", 
                         asset.GetName().c_str(), sound.frameCount);
            } else {
                LOG_SERVICE_ERRORF("ASSET_SERVICE", "Failed to finalize sound: %s (tried mono conversion)", asset.GetName().c_str());
                
                // Try the original wave as fallback
                sound = ::LoadSoundFromWave(waveData);
                if (sound.frameCount > 0) {
                    asset.SetSound(sound);
                    LOG_SERVICE_INFOF("ASSET_SERVICE", "Fallback sound creation succeeded: %s", asset.GetName().c_str());
                } else {
                    LOG_SERVICE_ERRORF("ASSET_SERVICE", "Both mono and stereo sound creation failed: %s", asset.GetName().c_str());
                }
            }
            
            // Clean up processed wave if we modified it
            if (processedWave.data != waveData.data) {
                ::UnloadWave(processedWave);
            }
        }
    }
    
    LOG_SERVICE_INFO("ASSET_SERVICE", "Asset finalization complete");
    LOG_SECTION_END("ASSET FINALIZATION");
}

} // namespace Elysium::Services