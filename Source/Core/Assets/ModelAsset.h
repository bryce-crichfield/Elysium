#pragma once

#include "Core/Graphics.h"
#include "Core/Asset.h"
#include "raylib.h"

namespace Elysium {

class ModelAsset : public AssetBase<ModelAsset> {
   public:
    using AssetBase::AssetBase;

    bool Load() override;
    void Unload() override;

    Model& GetData() { return model_; }

   private:
    Model model_{};
    ::Model nativeModel_{};
};

}  // namespace Elysium
