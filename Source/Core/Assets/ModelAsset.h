#pragma once

#include <memory>
#include "Core/Graphics.h"
#include "Core/Asset.h"

namespace Elysium {

// A 3D model file (.glb, .gltf, .obj) with its meshes, materials and embedded textures.
// Everything happens in Finalize: raylib reads the file and uploads it to the GPU in one
// step, which needs the main thread.
class ModelAsset : public AssetBase<ModelAsset> {
   public:
    using AssetBase::AssetBase;
    ~ModelAsset() override;

    bool Load() override;
    bool Finalize() override;
    bool NeedsFinalize() const override { return true; }
    void Unload() override;

    Model& GetData() { return model_; }

   private:
    struct Native;  // holds the raylib Model handle
    std::unique_ptr<Native> native_;
    Model model_{};
};

}  // namespace Elysium
