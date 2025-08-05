#include "Services/AssetService.h"
#include "raylib.h"

namespace Elysium::Services {

void AssetService::Initialize() {
    assetsByName_.clear();
    pathToName_.clear();
    TraceLog(LOG_INFO, "AssetService initialized");
}

void AssetService::Shutdown() {
    // Unload all assets and track memory deallocations
    for (auto& pair : assetsByName_) {
        Asset& asset = pair.second;
        if (asset.IsLoaded()) {
            asset.Unload();
            // Note: We can't track exact deallocation size for raylib assets
            TraceLog(LOG_INFO, "Unloaded asset: %s", asset.GetName().c_str());
        }
    }
    
    assetsByName_.clear();
    pathToName_.clear();
    TraceLog(LOG_INFO, "AssetService shutdown complete");
}

void AssetService::LoadAsset(const Asset& assetTemplate) {
    const std::string& name = assetTemplate.GetName();
    const std::string& path = assetTemplate.GetPath();
    
    // Check if already loaded by name
    if (IsAssetLoaded(name)) {
        TraceLog(LOG_INFO, "Asset already loaded: %s", name.c_str());
        return;
    }
    
    // Check path deduplication - if this path is already loaded under another name
    auto pathIt = pathToName_.find(path);
    if (pathIt != pathToName_.end()) {
        TraceLog(LOG_INFO, "Path %s already loaded as %s, skipping duplicate", path.c_str(), pathIt->second.c_str());
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
            TraceLog(LOG_INFO, "Successfully loaded asset: %s -> %s", name.c_str(), path.c_str());
        } else {
            TraceLog(LOG_INFO, "Raw data loaded for asset: %s -> %s (will finalize on main thread)", name.c_str(), path.c_str());
        }
    } else {
        TraceLog(LOG_WARNING, "Failed to load asset: %s -> %s", name.c_str(), path.c_str());
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
    
    TraceLog(LOG_WARNING, "Texture not found: %s", name.c_str());
    return Texture2D{0, 0, 0, 0, 0};
}

Sound AssetService::GetSound(const AssetName& name) {
    Asset* asset = GetAsset(name);
    if (asset && asset->GetType() == AssetType::SOUND) {
        return asset->GetSound();
    }
    
    TraceLog(LOG_WARNING, "Sound not found: %s", name.c_str());
    return Sound{nullptr, 0, 0, 0, 0};
}

Music AssetService::GetMusic(const AssetName& name) {
    Asset* asset = GetAsset(name);
    if (asset && asset->GetType() == AssetType::MUSIC) {
        return asset->GetMusic();
    }
    
    TraceLog(LOG_WARNING, "Music not found: %s", name.c_str());
    return Music{nullptr, 0, 0, 0, 0};
}

Font AssetService::GetFont(const AssetName& name) {
    Asset* asset = GetAsset(name);
    if (asset && asset->GetType() == AssetType::FONT) {
        return asset->GetFont();
    }
    
    TraceLog(LOG_WARNING, "Font not found: %s", name.c_str());
    return Font{};
}

Model AssetService::GetModel(const AssetName& name) {
    Asset* asset = GetAsset(name);
    if (asset && asset->GetType() == AssetType::MODEL) {
        return asset->GetModel();
    }
    
    TraceLog(LOG_WARNING, "Model not found: %s", name.c_str());
    return Model{};
}

Shader AssetService::GetShader(const AssetName& name) {
    Asset* asset = GetAsset(name);
    if (asset && asset->GetType() == AssetType::SHADER) {
        return asset->GetShader();
    }
    
    TraceLog(LOG_WARNING, "Shader not found: %s", name.c_str());
    return Shader{};
}

void AssetService::LoadAssetByType(Asset& asset) {
    const std::string& path = asset.GetPath();
    
    switch (asset.GetType()) {
        case AssetType::TEXTURE: {
            TraceLog(LOG_INFO, "Loading asset: %s -> %s", asset.GetName().c_str(), path.c_str());
            
            // Load image data only (thread-safe)
            Image image = ::LoadImage(path.c_str());
            if (image.data != nullptr) {
                TraceLog(LOG_INFO, "Image data loaded successfully: %dx%d, format: %d, mipmaps: %d", 
                    image.width, image.height, image.format, image.mipmaps);
                
                // Store image data for later texture creation on main thread
                asset.SetImageData(image);
                TraceLog(LOG_INFO, "Image data stored for: %s", path.c_str());
            } else {
                TraceLog(LOG_ERROR, "Failed to load image data: %s", path.c_str());
            }
            break;
        }
        
        case AssetType::SOUND: {
            TraceLog(LOG_INFO, "Loading sound directly (skipping wave separation): %s -> %s", asset.GetName().c_str(), path.c_str());
            
            // Try loading sound directly (not thread-safe but for testing)
            Sound sound = ::LoadSound(path.c_str());
            if (sound.frameCount > 0) {
                asset.SetSound(sound);
                TraceLog(LOG_INFO, "Direct sound load succeeded: %s (%d frames)", path.c_str(), sound.frameCount);
            } else {
                TraceLog(LOG_WARNING, "Direct sound load failed, falling back to wave method: %s", path.c_str());
                
                // Fallback to wave data approach
                Wave wave = ::LoadWave(path.c_str());
                if (wave.frameCount > 0) {
                    TraceLog(LOG_INFO, "Wave data loaded successfully: %d frames, %d Hz, %d channels", 
                             wave.frameCount, wave.sampleRate, wave.channels);
                    
                    // Store wave data for later sound creation on main thread
                    asset.SetWaveData(wave);
                    TraceLog(LOG_INFO, "Wave data stored for: %s", path.c_str());
                } else {
                    TraceLog(LOG_ERROR, "Both direct sound and wave data loading failed: %s", path.c_str());
                }
            }
            break;
        }
        
        case AssetType::MUSIC: {
            Music music = ::LoadMusicStream(path.c_str());
            if (music.frameCount > 0) {
                asset.SetMusic(music);
                TraceLog(LOG_INFO, "Loaded music: %s (%d frames)", path.c_str(), music.frameCount);
            } else {
                TraceLog(LOG_WARNING, "Failed to load music: %s", path.c_str());
            }
            break;
        }
        
        case AssetType::FONT: {
            Font font = ::LoadFont(path.c_str());
            if (font.texture.id != 0) {
                asset.SetFont(font);
                TraceLog(LOG_INFO, "Loaded font: %s", path.c_str());
            } else {
                TraceLog(LOG_WARNING, "Failed to load font: %s", path.c_str());
            }
            break;
        }
        
        case AssetType::MODEL: {
            Model model = ::LoadModel(path.c_str());
            if (model.meshCount > 0) {
                asset.SetModel(model);
                TraceLog(LOG_INFO, "Loaded model: %s (%d meshes)", path.c_str(), model.meshCount);
            } else {
                TraceLog(LOG_WARNING, "Failed to load model: %s", path.c_str());
            }
            break;
        }
        
        case AssetType::SHADER: {
            // For shaders, we might need vertex and fragment paths
            // For now, load as a fragment shader
            Shader shader = ::LoadShader(nullptr, path.c_str());
            if (shader.id != 0) {
                asset.SetShader(shader);
                TraceLog(LOG_INFO, "Loaded shader: %s (ID: %d)", path.c_str(), shader.id);
            } else {
                TraceLog(LOG_WARNING, "Failed to load shader: %s", path.c_str());
            }
            break;
        }
        
        default:
            TraceLog(LOG_WARNING, "Unknown asset type for: %s", path.c_str());
            break;
    }
}

void AssetService::FinalizeAssets() {
    TraceLog(LOG_INFO, "Finalizing assets on main thread...");
    
    for (auto& pair : assetsByName_) {
        Asset& asset = pair.second;
        
        // Convert image data to texture
        if (asset.HasImageData() && !asset.IsLoaded() && asset.GetType() == AssetType::TEXTURE) {
            Image imageData = asset.GetImageData();
            Texture2D texture = ::LoadTextureFromImage(imageData);
            
            if (texture.id != 0) {
                asset.SetTexture(texture);
                TraceLog(LOG_INFO, "Finalized texture: %s (ID: %d, %dx%d)", 
                         asset.GetName().c_str(), texture.id, texture.width, texture.height);
            } else {
                TraceLog(LOG_ERROR, "Failed to finalize texture: %s", asset.GetName().c_str());
            }
        }
        
        // Convert wave data to sound
        if (asset.HasWaveData() && !asset.IsLoaded() && asset.GetType() == AssetType::SOUND) {
            Wave waveData = asset.GetWaveData();
            
            TraceLog(LOG_INFO, "Attempting to create sound from wave: %s (%d frames, %d Hz, %d channels, %d sampleSize)", 
                     asset.GetName().c_str(), waveData.frameCount, waveData.sampleRate, waveData.channels, waveData.sampleSize);
            
            // Try converting to mono if stereo
            Wave processedWave = waveData;
            if (waveData.channels == 2) {
                TraceLog(LOG_INFO, "Converting stereo to mono for: %s", asset.GetName().c_str());
                ::WaveFormat(&processedWave, 44100, 16, 1);
            }
            
            Sound sound = ::LoadSoundFromWave(processedWave);
            
            if (sound.frameCount > 0) {
                asset.SetSound(sound);
                TraceLog(LOG_INFO, "Finalized sound: %s (%d frames)", 
                         asset.GetName().c_str(), sound.frameCount);
            } else {
                TraceLog(LOG_ERROR, "Failed to finalize sound: %s (tried mono conversion)", asset.GetName().c_str());
                
                // Try the original wave as fallback
                sound = ::LoadSoundFromWave(waveData);
                if (sound.frameCount > 0) {
                    asset.SetSound(sound);
                    TraceLog(LOG_INFO, "Fallback sound creation succeeded: %s", asset.GetName().c_str());
                } else {
                    TraceLog(LOG_ERROR, "Both mono and stereo sound creation failed: %s", asset.GetName().c_str());
                }
            }
            
            // Clean up processed wave if we modified it
            if (processedWave.data != waveData.data) {
                ::UnloadWave(processedWave);
            }
        }
    }
    
    TraceLog(LOG_INFO, "Asset finalization complete");
}

} // namespace Elysium::Services