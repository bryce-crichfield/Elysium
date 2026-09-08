#pragma once

#include <memory>
#include "Core/Graphics.h"
#include "Core/Asset.h"

namespace Elysium {

class ShaderAsset : public AssetBase<ShaderAsset> {
   public:
    using AssetBase::AssetBase;
    ~ShaderAsset() override;

    bool Load() override;
    void Unload() override;

    Shader& GetData() { return shader_; }

   private:
    struct Native;  // holds the raylib Shader handle for Unload()
    std::unique_ptr<Native> native_;
    Shader shader_{};
};

}  // namespace Elysium
