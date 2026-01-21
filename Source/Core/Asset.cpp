#include "Asset.h"
#include <stdexcept>

namespace Elysium {

Asset::Asset(AssetType type, const AssetName& name, const std::string& path) : type_(type), name_(name), path_(Path(path)), loaded_(false) {
}

Texture2D Asset::GetTexture() const {
    if (type_ != AssetType::TEXTURE || !loaded_) {
        return {};
    }
    return std::get<Texture2D>(data_);
}

Sound Asset::GetSound() const {
    if (type_ != AssetType::SOUND || !loaded_) {
        return {};
    }
    return std::get<Sound>(data_);
}

Music Asset::GetMusic() const {
    if (type_ != AssetType::MUSIC || !loaded_) {
        return {};
    }
    return std::get<Music>(data_);
}

Font Asset::GetFont() const {
    if (type_ != AssetType::FONT || !loaded_) {
        return {};
    }
    return std::get<Font>(data_);
}

Model Asset::GetModel() const {
    if (type_ != AssetType::MODEL || !loaded_) {
        return {};
    }
    return std::get<Model>(data_);
}

Shader Asset::GetShader() const {
    if (type_ != AssetType::SHADER || !loaded_) {
        return {};
    }
    return std::get<Shader>(data_);
}

Sprite Asset::GetSprite() const {
    if (type_ != AssetType::SPRITE || !loaded_) {
        return {};
    }

    return std::get<Sprite>(data_);
}

void Asset::SetTexture(const Texture2D& texture) {
    if (type_ == AssetType::TEXTURE) {
        data_ = texture;
        loaded_ = true;
    }
}

void Asset::SetSound(const Sound& sound) {
    if (type_ == AssetType::SOUND) {
        data_ = sound;
        loaded_ = true;
    }
}

void Asset::SetMusic(const Music& music) {
    if (type_ == AssetType::MUSIC) {
        data_ = music;
        loaded_ = true;
    }
}

void Asset::SetFont(const Font& font) {
    if (type_ == AssetType::FONT) {
        data_ = font;
        loaded_ = true;
    }
}

void Asset::SetModel(const Model& model) {
    if (type_ == AssetType::MODEL) {
        data_ = model;
        loaded_ = true;
    }
}

void Asset::SetShader(const Shader& shader) {
    if (type_ == AssetType::SHADER) {
        data_ = shader;
        loaded_ = true;
    }
}

void Asset::SetImageData(const Image& image) {
    imageData_ = image;
    hasImageData_ = true;
}

void Asset::SetWaveData(const Wave& wave) {
    waveData_ = wave;
    hasWaveData_ = true;
}

void Asset::SetSprite(const Sprite& sprite) {
    if (type_ == AssetType::SPRITE) {
        data_ = sprite;
        loaded_ = true;
    }
}

void Asset::Unload() {
    if (loaded_) {
        switch (type_) {
            case AssetType::TEXTURE:
                UnloadTexture(std::get<Texture2D>(data_));
                break;
            case AssetType::SOUND:
                UnloadSound(std::get<Sound>(data_));
                break;
            case AssetType::MUSIC:
                UnloadMusicStream(std::get<Music>(data_));
                break;
            case AssetType::FONT:
                UnloadFont(std::get<Font>(data_));
                break;
            case AssetType::MODEL:
                UnloadModel(std::get<Model>(data_));
                break;
            case AssetType::SHADER:
                UnloadShader(std::get<Shader>(data_));
                break;
        }
        loaded_ = false;
    }

    // Unload raw data
    if (hasImageData_) {
        ::UnloadImage(imageData_);
        hasImageData_ = false;
    }

    if (hasWaveData_) {
        ::UnloadWave(waveData_);
        hasWaveData_ = false;
    }
}

}  // namespace Elysium
