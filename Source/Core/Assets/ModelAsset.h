#pragma once

#include <memory>
#include "Core/Animation.h"
#include "Core/Graphics.h"
#include "Core/Asset.h"

namespace Elysium {

// A 3D model file with its meshes, materials and textures.
//  - .glb, .gltf, .obj: raylib reads the file and uploads it to the GPU in one step, in
//    Finalize, which needs the main thread.
//  - .mesh (the baker's format): read in Load, off the main thread, uploaded in Finalize.
//    A .skel beside it (same name) makes it skinned: Model::skeleton, posed by an
//    AnimationComponent. Its textures are files beside it, named by the .mesh.
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
    struct BakedMesh;  // a .mesh read in Load, waiting for Finalize
    std::unique_ptr<Native> native_;
    std::unique_ptr<BakedMesh> baked_;
    std::unique_ptr<Skeleton> skeleton_;
    Model model_{};
};

}  // namespace Elysium
