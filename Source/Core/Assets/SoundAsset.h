#pragma once

#include "Core/Asset.h"
#include "raylib.h"

namespace Elysium {

class SoundAsset : public IAsset {
public:
    explicit SoundAsset(Path path) : IAsset(std::move(path)) {}

    void LoadData() override {
        wave_ = ::LoadWave(path_.c_str());
        if (wave_.frameCount > 0) hasWave_ = true;
    }

    void Finalize() override {
        if (!hasWave_) return;
        Wave processed = wave_;
        bool converted = false;
        if (wave_.channels == 2) {
            ::WaveFormat(&processed, 44100, 16, 1);
            converted = true;
        }
        sound_ = ::LoadSoundFromWave(processed);
        if (sound_.frameCount == 0) sound_ = ::LoadSoundFromWave(wave_);
        if (converted && processed.data != wave_.data) ::UnloadWave(processed);
        ::UnloadWave(wave_);
        wave_ = {};
        hasWave_ = false;
        if (sound_.frameCount > 0) loaded_ = true;
    }

    bool NeedsFinalization() const override { return hasWave_; }

    void Unload() override {
        if (loaded_) { ::UnloadSound(sound_); sound_ = {}; loaded_ = false; }
        if (hasWave_) { ::UnloadWave(wave_); wave_ = {}; hasWave_ = false; }
    }

    AssetType GetType() const override { return AssetType::SOUND; }
    Sound Get() const { return sound_; }

private:
    Wave wave_{};
    bool hasWave_ = false;
    Sound sound_{};
};

}  // namespace Elysium
