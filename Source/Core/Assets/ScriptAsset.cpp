#include "Core/Assets/ScriptAsset.h"
#include "Core/Asset.h"
#include "Services/LogService.h"
#include "raylib.h"

namespace Elysium {

bool ScriptAsset::Load() {
    char* text = ::LoadFileText(GetPath().c_str());
    if (!text) {
        LOG_ERRORF("ScriptAsset", "Failed to load script: %s", GetPath().c_str());
        return false;
    }

    script_ = Script{std::string(text), GetPath()};
    ::UnloadFileText(text);
    SetLoaded(true);
    LOG_DEBUGF("ScriptAsset", "Script loaded: %s", GetPath().c_str());
    return true;
}

REGISTER_ASSET_TYPE(Script, ScriptAsset);

}  // namespace Elysium
