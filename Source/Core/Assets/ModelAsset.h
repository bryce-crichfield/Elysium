#pragma once

#include <memory>
#include <string>
#include "Core/Animation.h"
#include "Core/Graphics.h"
#include "Core/Asset.h"

namespace Elysium {

// A 3D model file with its meshes, materials and textures. Read and decoded in Load, off
// the main thread; Finalize (main thread) only uploads to the GPU.
//  - .glb, .gltf: read with cgltf (ReadGltf), each node's transform baked in.
//  - .mesh (the baker's format), with its textures (files beside it).
//  - .obj: raylib reads and uploads it in one step, in Finalize, on the main thread.
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
    struct BakedMesh;  // a model read in Load, waiting for Finalize
    std::unique_ptr<BakedMesh> ReadGltf(const std::string& path);
    std::unique_ptr<Native> native_;
    std::unique_ptr<BakedMesh> baked_;
    std::unique_ptr<Skeleton> skeleton_;
    Model model_{};
};

}  // namespace Elysium
