#pragma once

#include "Core/Audio.h"
#include "Core/Asset.h"

namespace Elysium {

// A sound file decoded whole into memory, in the mixer's format (Core/Audio.h). Nothing to
// upload, so it's ready as soon as the background Load finishes.
class SoundAsset : public AssetBase<SoundAsset> {
   public:
    using AssetBase::AssetBase;

    bool Load() override;
    void Unload() override;

    Sound& GetData() { return sound_; }

   private:
    Sound sound_{};
};

}  // namespace Elysium
