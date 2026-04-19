#pragma once

#include "Core/Asset.h"
#include "raylib.h"

namespace Elysium {

class ShaderAsset : public IAsset {
public:
    explicit ShaderAsset(Path path) : IAsset(std::move(path)) {}

    void LoadData() override {
        shader_ = ::LoadShader(nullptr, path_.c_str());
        if (shader_.id != 0) loaded_ = true;
    }

    void Unload() override {
        if (loaded_) { ::UnloadShader(shader_); shader_ = {}; loaded_ = false; }
    }

    AssetType GetType() const override { return AssetType::SHADER; }
    Shader Get() const { return shader_; }

private:
    Shader shader_{};
};

}  // namespace Elysium
