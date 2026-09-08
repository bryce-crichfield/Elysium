#include "Core/Assets/ModelAsset.h"
#include "Core/Asset.h"
#include "Services/LogService.h"
#include "raylib.h"

namespace Elysium {

struct ModelAsset::Native {
    ::Model model{};
};

ModelAsset::~ModelAsset() = default;

bool ModelAsset::Load() {
    ::Model model = ::LoadModel(GetPath().c_str());
    if (model.meshCount == 0) {
        LOG_ERRORF("ModelAsset", "Failed to load model: %s", GetPath().c_str());
        return false;
    }

    native_ = std::make_unique<Native>();
    native_->model = model;
    model_ = Model{model.meshCount, model.materialCount};
    SetLoaded(true);
    LOG_DEBUGF("ModelAsset", "Model loaded: %d meshes", model.meshCount);
    return true;
}

void ModelAsset::Unload() {
    if (IsLoaded() && native_) {
        ::UnloadModel(native_->model);
        SetLoaded(false);
    }
    native_.reset();
}

REGISTER_ASSET_TYPE(Model, ModelAsset);

}  // namespace Elysium
