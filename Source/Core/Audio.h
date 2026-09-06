#pragma once

namespace Elysium {

    // Identity-only — SoundAsset/MusicAsset keep the real raylib resource privately.
    struct Sound {
        unsigned int frameCount = 0;
    };

    struct Music {
        unsigned int frameCount = 0;
        bool looping = false;
    };

}
