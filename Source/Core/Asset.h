#pragma once

#include <string>
#include <variant>
#include "Utilities/Path.h"
#include "Sprite.h"
#include "raylib.h"

#ifndef ASSETS_PATH
#define ASSETS_PATH "./Assets/"
#endif

namespace Elysium {

using AssetName = std::string;
using Script = std::string;

enum class AssetType {
    TEXTURE,
    SOUND,
    MUSIC,
    FONT,
    MODEL,
    SHADER,
    SPRITE,
    SCRIPT
};

class Asset {
   public:
    Asset() = default;
    Asset(AssetType type, const AssetName& name, const std::string& path);
    ~Asset() = default;

    AssetType GetType() const { return type_; }
    const AssetName& GetName() const { return name_; }
    std::string GetPath() const {
        return path_.GetFullPath();
    }
    const Path& GetAssetPath() const { return path_; }
    bool IsLoaded() const { return loaded_; }
    bool HasImageData() const { return hasImageData_; }
    bool HasWaveData() const { return hasWaveData_; }

    Texture2D GetTexture() const;
    Sound GetSound() const;
    Music GetMusic() const;
    Font GetFont() const;
    Model GetModel() const;
    Shader GetShader() const;
    Image GetImageData() const { return imageData_; }
    Wave GetWaveData() const { return waveData_; }
    Sprite GetSprite() const;
    Script GetScript() const;

    void SetTexture(const Texture2D& texture);
    void SetSound(const Sound& sound);
    void SetMusic(const Music& music);
    void SetFont(const Font& font);
    void SetModel(const Model& model);
    void SetShader(const Shader& shader);
    void SetImageData(const Image& image);
    void SetWaveData(const Wave& wave);
    void SetSprite(const Sprite& sprite);
    void SetScript(const Script& script);

    void Unload();

   private:
    AssetType type_;
    std::string name_;
    Path path_;
    bool loaded_ = false;
    bool hasImageData_ = false;
    bool hasWaveData_ = false;

    std::variant<Texture2D, Sound, Music, Font, Model, Shader, Sprite, Script> data_;
    Image imageData_{};
    Wave waveData_{};
};

}  // namespace Elysium
