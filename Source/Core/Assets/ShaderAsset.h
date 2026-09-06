#pragma once

#include "Core/Graphics.h"
#include "Core/Asset.h"
#include "raylib.h"

namespace Elysium {

class ShaderAsset : public AssetBase<ShaderAsset> {
   public:
    using AssetBase::AssetBase;

    bool Load() override;
    void Unload() override;

    Shader& GetData() { return shader_; }

   private:
    Shader shader_{};
    ::Shader nativeShader_{};
};

}  // namespace Elysium
