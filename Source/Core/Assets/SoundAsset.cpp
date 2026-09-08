#include "Core/Assets/SoundAsset.h"
#include "Core/Asset.h"
#include "Services/LogService.h"
#include "raylib.h"

namespace Elysium {

struct SoundAsset::Native {
    ::Wave wave{};
    bool hasWave = false;
    ::Sound sound{};
};

SoundAsset::~SoundAsset() = default;

bool SoundAsset::Load() {
    ::Wave wave = ::LoadWave(GetPath().c_str());
    if (wave.frameCount == 0) {
        LOG_ERRORF("SoundAsset", "Failed to load wave data: %s", GetPath().c_str());
        return false;
    }

    LOG_DEBUGF("SoundAsset", "Wave data loaded: %d frames, %d Hz, %d channels",
               wave.frameCount, wave.sampleRate, wave.channels);
    native_ = std::make_unique<Native>();
    native_->wave = wave;
    native_->hasWave = true;
    return true;
}

bool SoundAsset::Finalize() {
    if (!native_ || !native_->hasWave) return false;

    ::Wave& waveData = native_->wave;
    LOG_INFOF("SoundAsset", "Creating sound from wave: %s (%d frames, %d Hz, %d channels)",
              GetPath().c_str(), waveData.frameCount, waveData.sampleRate, waveData.channels);

    ::Wave processedWave = waveData;
    if (waveData.channels == 2) {
        LOG_INFOF("SoundAsset", "Converting stereo to mono for: %s", GetPath().c_str());
        ::WaveFormat(&processedWave, 44100, 16, 1);
    }

    ::Sound sound = ::LoadSoundFromWave(processedWave);
    if (sound.frameCount == 0) {
        LOG_ERRORF("SoundAsset", "Failed to finalize sound: %s (tried mono conversion)", GetPath().c_str());
        sound = ::LoadSoundFromWave(waveData);
        if (sound.frameCount > 0) {
            LOG_INFOF("SoundAsset", "Fallback sound creation succeeded: %s", GetPath().c_str());
        }
    }

    if (processedWave.data != waveData.data) {
        ::UnloadWave(processedWave);
    }
    ::UnloadWave(waveData);
    native_->hasWave = false;

    if (sound.frameCount == 0) {
        LOG_ERRORF("SoundAsset", "Both mono and stereo sound creation failed: %s", GetPath().c_str());
        return false;
    }

    native_->sound = sound;
    sound_ = Sound{sound.frameCount};
    SetLoaded(true);
    LOG_INFOF("SoundAsset", "Finalized sound: %s (%d frames)", GetPath().c_str(), sound.frameCount);
    return true;
}

void SoundAsset::Unload() {
    if (IsLoaded() && native_) {
        ::UnloadSound(native_->sound);
        SetLoaded(false);
    }
    if (native_ && native_->hasWave) {
        ::UnloadWave(native_->wave);
        native_->hasWave = false;
    }
    native_.reset();
}

REGISTER_ASSET_TYPE(Sound, SoundAsset);

}  // namespace Elysium
