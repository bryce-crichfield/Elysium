#include "Core/Assets/ModelAsset.h"
#include "Core/Asset.h"
#include "Services/LogService.h"

namespace Elysium {

bool ModelAsset::Load() {
    ::Model model = ::LoadModel(GetPath().c_str());
    if (model.meshCount == 0) {
        LOG_ERRORF("ModelAsset", "Failed to load model: %s", GetPath().c_str());
        return false;
    }

    nativeModel_ = model;
    model_ = Model{model.meshCount, model.materialCount};
    SetLoaded(true);
    LOG_DEBUGF("ModelAsset", "Model loaded: %d meshes", model.meshCount);
    return true;
}

void ModelAsset::Unload() {
    if (IsLoaded()) {
        ::UnloadModel(nativeModel_);
        SetLoaded(false);
    }
}

REGISTER_ASSET_TYPE(Model, ModelAsset);

}  // namespace Elysium
