#pragma once

#include "raylib.h"
#include <string>
#include <variant>

namespace Elysium {

enum class AssetType {
    TEXTURE,
    SOUND,
    MUSIC,
    FONT,
    MODEL,
    SHADER
};

class Asset {
public:
    Asset() = default;
    Asset(AssetType type, const std::string& path);
    ~Asset() = default;

    AssetType GetType() const { return type_; }
    const std::string& GetPath() const { return path_; }
    bool IsLoaded() const { return loaded_; }
    
    Texture2D GetTexture() const;
    Sound GetSound() const;
    Music GetMusic() const;
    Font GetFont() const;
    Model GetModel() const;
    Shader GetShader() const;
    
    void SetTexture(const Texture2D& texture);
    void SetSound(const Sound& sound);
    void SetMusic(const Music& music);
    void SetFont(const Font& font);
    void SetModel(const Model& model);
    void SetShader(const Shader& shader);
    
    void Unload();

private:
    AssetType type_;
    std::string path_;
    bool loaded_ = false;
    
    std::variant<Texture2D, Sound, Music, Font, Model, Shader> data_;
};

} // namespace Elysium