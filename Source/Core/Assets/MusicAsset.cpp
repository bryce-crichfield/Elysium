#include "Core/Assets/MusicAsset.h"
#include "Core/Asset.h"
#include "Services/LogService.h"

namespace Elysium {

bool MusicAsset::Load() {
    ::Music music = ::LoadMusicStream(GetPath().c_str());
    if (music.frameCount == 0) {
        LOG_ERRORF("MusicAsset", "Failed to load music: %s", GetPath().c_str());
        return false;
    }

    nativeMusic_ = music;
    music_ = Music{music.frameCount, music.looping};
    SetLoaded(true);
    LOG_DEBUGF("MusicAsset", "Music loaded: %d frames", music.frameCount);
    return true;
}

void MusicAsset::Unload() {
    if (IsLoaded()) {
        ::UnloadMusicStream(nativeMusic_);
        SetLoaded(false);
    }
}

REGISTER_ASSET_TYPE(Music, MusicAsset);

}  // namespace Elysium
