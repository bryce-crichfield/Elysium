#pragma once

#include "Core/Asset.h"
#include "raylib.h"

namespace Elysium {

class MusicAsset : public IAsset {
public:
    explicit MusicAsset(Path path) : IAsset(std::move(path)) {}

    void LoadData() override {
        music_ = ::LoadMusicStream(path_.c_str());
        if (music_.frameCount > 0) loaded_ = true;
    }

    void Unload() override {
        if (loaded_) { ::UnloadMusicStream(music_); music_ = {}; loaded_ = false; }
    }

    AssetType GetType() const override { return AssetType::MUSIC; }
    Music Get() const { return music_; }

private:
    Music music_{};
};

}  // namespace Elysium
