#include "Core/Assets/ShaderAsset.h"
#include "Core/Asset.h"
#include "Services/LogService.h"
#include "raylib.h"

namespace Elysium {

struct ShaderAsset::Native {
    ::Shader shader{};
};

ShaderAsset::~ShaderAsset() = default;

bool ShaderAsset::Load() {
    ::Shader shader = ::LoadShader(nullptr, GetPath().c_str());
    if (shader.id == 0) {
        LOG_ERRORF("ShaderAsset", "Failed to load shader: %s", GetPath().c_str());
        return false;
    }

    native_ = std::make_unique<Native>();
    native_->shader = shader;
    shader_ = Shader{shader.id};
    SetLoaded(true);
    LOG_DEBUGF("ShaderAsset", "Shader loaded: ID %d", shader.id);
    return true;
}

void ShaderAsset::Unload() {
    if (IsLoaded() && native_) {
        ::UnloadShader(native_->shader);
        SetLoaded(false);
    }
    native_.reset();
}

REGISTER_ASSET_TYPE(Shader, ShaderAsset);

}  // namespace Elysium
