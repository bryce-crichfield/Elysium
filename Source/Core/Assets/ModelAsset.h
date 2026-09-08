#pragma once

#include <memory>
#include "Core/Graphics.h"
#include "Core/Asset.h"

namespace Elysium {

class ModelAsset : public AssetBase<ModelAsset> {
   public:
    using AssetBase::AssetBase;
    ~ModelAsset() override;

    bool Load() override;
    void Unload() override;

    Model& GetData() { return model_; }

   private:
    struct Native;  // holds the raylib Model handle for Unload()
    std::unique_ptr<Native> native_;
    Model model_{};
};

}  // namespace Elysium
