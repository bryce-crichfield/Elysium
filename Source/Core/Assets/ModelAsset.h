#pragma once

#include "Core/Asset.h"
#include "raylib.h"

namespace Elysium {

class ModelAsset : public IAsset {
public:
    explicit ModelAsset(Path path) : IAsset(std::move(path)) {}

    void LoadData() override {
        model_ = ::LoadModel(path_.c_str());
        if (model_.meshCount > 0) loaded_ = true;
    }

    void Unload() override {
        if (loaded_) { ::UnloadModel(model_); model_ = {}; loaded_ = false; }
    }

    AssetType GetType() const override { return AssetType::MODEL; }
    Model Get() const { return model_; }

private:
    Model model_{};
};

}  // namespace Elysium
