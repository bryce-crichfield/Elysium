#pragma once

#include <memory>
#include "Core/Audio.h"
#include "Core/Asset.h"

namespace Elysium {

class MusicAsset : public AssetBase<MusicAsset> {
   public:
    using AssetBase::AssetBase;
    ~MusicAsset() override;

    bool Load() override;
    void Unload() override;

    Music& GetData() { return music_; }

   private:
    struct Native;  // holds the raylib Music stream for Unload()
    std::unique_ptr<Native> native_;
    Music music_{};
};

}  // namespace Elysium
