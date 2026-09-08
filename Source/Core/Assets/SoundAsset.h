#pragma once

#include <memory>
#include "Core/Audio.h"
#include "Core/Asset.h"

namespace Elysium {

class SoundAsset : public AssetBase<SoundAsset> {
   public:
    using AssetBase::AssetBase;
    ~SoundAsset() override;

    bool Load() override;      // background thread: reads raw wave bytes
    bool Finalize() override;  // main thread: uploads to the audio device
    bool NeedsFinalize() const override { return true; }
    void Unload() override;

    Sound& GetData() { return sound_; }

   private:
    struct Native;  // holds the raylib Wave (Load->Finalize) and Sound handle
    std::unique_ptr<Native> native_;
    Sound sound_{};
};

}  // namespace Elysium
