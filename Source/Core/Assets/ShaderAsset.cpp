#include "Core/Assets/ShaderAsset.h"
#include "Core/Asset.h"
#include "Services/LogService.h"

namespace Elysium {

bool ShaderAsset::Load() {
    ::Shader shader = ::LoadShader(nullptr, GetPath().c_str());
    if (shader.id == 0) {
        LOG_ERRORF("ShaderAsset", "Failed to load shader: %s", GetPath().c_str());
        return false;
    }

    nativeShader_ = shader;
    shader_ = Shader{shader.id};
    SetLoaded(true);
    LOG_DEBUGF("ShaderAsset", "Shader loaded: ID %d", shader.id);
    return true;
}

void ShaderAsset::Unload() {
    if (IsLoaded()) {
        ::UnloadShader(nativeShader_);
        SetLoaded(false);
    }
}

REGISTER_ASSET_TYPE(Shader, ShaderAsset);

}  // namespace Elysium
