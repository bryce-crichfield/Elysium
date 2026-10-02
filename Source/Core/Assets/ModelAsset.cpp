#include "Core/Assets/ModelAsset.h"
#include "Core/Asset.h"
#include "Services/LogService.h"
#include "raylib.h"

#include <filesystem>

namespace Elysium {

struct ModelAsset::Native {
    ::Model model{};
};

ModelAsset::~ModelAsset() = default;

bool ModelAsset::Load() {
    std::error_code ec;
    if (!std::filesystem::exists(GetPath().c_str(), ec)) {
        LOG_ERRORF("ModelAsset", "Model file not found: %s", GetPath().c_str());
        return false;
    }
    return true;
}

bool ModelAsset::Finalize() {
    ::Model model = ::LoadModel(GetPath().c_str());
    if (model.meshCount == 0) {
        LOG_ERRORF("ModelAsset", "Failed to load model: %s", GetPath().c_str());
        ::UnloadModel(model);
        return false;
    }

    native_ = std::make_unique<Native>();
    native_->model = model;
    const ::BoundingBox box = ::GetModelBoundingBox(model);
    model_ = Model{model.meshCount, model.materialCount,
                   {box.min.x, box.min.y, box.min.z}, {box.max.x, box.max.y, box.max.z}, &native_->model};
    SetLoaded(true);
    LOG_DEBUGF("ModelAsset", "Model loaded: %s (%d meshes, %.2f x %.2f x %.2f)", GetPath().c_str(), model.meshCount,
               box.max.x - box.min.x, box.max.y - box.min.y, box.max.z - box.min.z);
    return true;
}

void ModelAsset::Unload() {
    if (native_) ::UnloadModel(native_->model);
    native_.reset();
    model_ = Model{};
    SetLoaded(false);
}

REGISTER_ASSET_TYPE(Model, ModelAsset);

}  // namespace Elysium
