#include "Core/Assets/SoundAsset.h"
#include "Core/Asset.h"
#include "Services/LogService.h"

namespace Elysium {

bool SoundAsset::Load() {
    Wave wave = ::LoadWave(GetPath().c_str());
    if (wave.frameCount == 0) {
        LOG_ERRORF("SoundAsset", "Failed to load wave data: %s", GetPath().c_str());
        return false;
    }

    LOG_DEBUGF("SoundAsset", "Wave data loaded: %d frames, %d Hz, %d channels",
               wave.frameCount, wave.sampleRate, wave.channels);
    waveData_ = wave;
    hasWaveData_ = true;
    return true;
}

bool SoundAsset::Finalize() {
    if (!hasWaveData_) return false;

    LOG_INFOF("SoundAsset", "Creating sound from wave: %s (%d frames, %d Hz, %d channels)",
              GetPath().c_str(), waveData_.frameCount, waveData_.sampleRate, waveData_.channels);

    Wave processedWave = waveData_;
    if (waveData_.channels == 2) {
        LOG_INFOF("SoundAsset", "Converting stereo to mono for: %s", GetPath().c_str());
        ::WaveFormat(&processedWave, 44100, 16, 1);
    }

    ::Sound sound = ::LoadSoundFromWave(processedWave);
    if (sound.frameCount == 0) {
        LOG_ERRORF("SoundAsset", "Failed to finalize sound: %s (tried mono conversion)", GetPath().c_str());
        sound = ::LoadSoundFromWave(waveData_);
        if (sound.frameCount > 0) {
            LOG_INFOF("SoundAsset", "Fallback sound creation succeeded: %s", GetPath().c_str());
        }
    }

    if (processedWave.data != waveData_.data) {
        ::UnloadWave(processedWave);
    }
    ::UnloadWave(waveData_);
    hasWaveData_ = false;

    if (sound.frameCount == 0) {
        LOG_ERRORF("SoundAsset", "Both mono and stereo sound creation failed: %s", GetPath().c_str());
        return false;
    }

    nativeSound_ = sound;
    sound_ = Sound{sound.frameCount};
    SetLoaded(true);
    LOG_INFOF("SoundAsset", "Finalized sound: %s (%d frames)", GetPath().c_str(), sound.frameCount);
    return true;
}

void SoundAsset::Unload() {
    if (IsLoaded()) {
        ::UnloadSound(nativeSound_);
        SetLoaded(false);
    }
    if (hasWaveData_) {
        ::UnloadWave(waveData_);
        hasWaveData_ = false;
    }
}

REGISTER_ASSET_TYPE(Sound, SoundAsset);

}  // namespace Elysium
