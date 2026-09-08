#include "Core/Assets/MusicAsset.h"
#include "Core/Asset.h"
#include "Services/LogService.h"
#include "raylib.h"

namespace Elysium {

struct MusicAsset::Native {
    ::Music music{};
};

MusicAsset::~MusicAsset() = default;

bool MusicAsset::Load() {
    ::Music music = ::LoadMusicStream(GetPath().c_str());
    if (music.frameCount == 0) {
        LOG_ERRORF("MusicAsset", "Failed to load music: %s", GetPath().c_str());
        return false;
    }

    native_ = std::make_unique<Native>();
    native_->music = music;
    music_ = Music{music.frameCount, music.looping};
    SetLoaded(true);
    LOG_DEBUGF("MusicAsset", "Music loaded: %d frames", music.frameCount);
    return true;
}

void MusicAsset::Unload() {
    if (IsLoaded() && native_) {
        ::UnloadMusicStream(native_->music);
        SetLoaded(false);
    }
    native_.reset();
}

REGISTER_ASSET_TYPE(Music, MusicAsset);

}  // namespace Elysium
