#pragma once

#include "Core/Asset.h"
#include "Core/Prefab.h"

namespace Elysium {

class PrefabAsset : public AssetBase<PrefabAsset> {
   public:
    using AssetBase::AssetBase;

    bool Load() override;
    void Unload() override {}

    Prefab& GetData() { return prefab_; }

   private:
    Prefab prefab_;
};

}  // namespace Elysium
