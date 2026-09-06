#pragma once

#include "Core/Audio.h"
#include "Core/Asset.h"
#include "raylib.h"

namespace Elysium {

class SoundAsset : public AssetBase<SoundAsset> {
   public:
    using AssetBase::AssetBase;

    bool Load() override;      // background thread: reads raw wave bytes
    bool Finalize() override;  // main thread: uploads to the audio device
    bool NeedsFinalize() const override { return true; }
    void Unload() override;

    Sound& GetData() { return sound_; }

   private:
    Sound sound_{};
    Wave waveData_{};
    bool hasWaveData_ = false;
    ::Sound nativeSound_{};
};

}  // namespace Elysium
