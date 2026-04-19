#pragma once

#include <memory>
#include "Core/Asset.h"
#include "Core/Assets/TextureAsset.h"
#include "Core/Assets/SoundAsset.h"
#include "Core/Assets/MusicAsset.h"
#include "Core/Assets/FontAsset.h"
#include "Core/Assets/ModelAsset.h"
#include "Core/Assets/ShaderAsset.h"
#include "Core/Assets/SpriteAsset.h"
#include "Core/Assets/ScriptAsset.h"
#include "Core/Assets/TileAsset.h"
#include "Core/Assets/CharacterAsset.h"

namespace Elysium {

// The only place AssetType is switched on. Adding a new asset type means:
// 1. Create a concrete IAsset subclass
// 2. Add one case here
inline std::shared_ptr<IAsset> CreateAsset(AssetType type, Path path) {
    switch (type) {
        case AssetType::TEXTURE:   return std::make_shared<TextureAsset>(path);
        case AssetType::SOUND:     return std::make_shared<SoundAsset>(path);
        case AssetType::MUSIC:     return std::make_shared<MusicAsset>(path);
        case AssetType::FONT:      return std::make_shared<FontAsset>(path);
        case AssetType::MODEL:     return std::make_shared<ModelAsset>(path);
        case AssetType::SHADER:    return std::make_shared<ShaderAsset>(path);
        case AssetType::SPRITE:    return std::make_shared<SpriteAsset>(path);
        case AssetType::SCRIPT:    return std::make_shared<ScriptAsset>(path);
        case AssetType::TILE:      return std::make_shared<TileAsset>(path);
        case AssetType::CHARACTER: return std::make_shared<CharacterAsset>(path);
    }
    return nullptr;
}

}  // namespace Elysium
