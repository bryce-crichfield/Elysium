#pragma once

#include "Core/Audio.h"
#include "Core/Asset.h"
#include "raylib.h"

namespace Elysium {

class MusicAsset : public AssetBase<MusicAsset> {
   public:
    using AssetBase::AssetBase;

    bool Load() override;
    void Unload() override;

    Music& GetData() { return music_; }

   private:
    Music music_{};
    ::Music nativeMusic_{};
};

}  // namespace Elysium
