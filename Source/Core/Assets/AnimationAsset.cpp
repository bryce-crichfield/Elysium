#include "Core/Assets/AnimationAsset.h"
#include "Services/LogService.h"

namespace Elysium {

bool AnimationAsset::Load() {
    std::string error;
    if (!animation_.Load(GetPath().GetFullPath(), error)) {
        LOG_ERRORF("AnimationAsset", "Failed to load animation %s: %s", GetPath().c_str(), error.c_str());
        return false;
    }
    LOG_DEBUGF("AnimationAsset", "Animation loaded: %s (%d frames, %d tracks, %.2fs)", GetPath().c_str(),
               animation_.frameCount, (int)animation_.tracks.size(), animation_.duration);
    SetLoaded(true);
    return true;
}

void AnimationAsset::Unload() {
    animation_ = Animation{};
    SetLoaded(false);
}

REGISTER_ASSET_TYPE(Animation, AnimationAsset);

}  // namespace Elysium
