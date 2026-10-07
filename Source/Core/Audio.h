#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Elysium {

    namespace Audio {
        // The one format the mixer speaks: every clip is decoded and resampled to it on load.
        constexpr int SampleRate = 48000;
        constexpr int Channels = 2;  // interleaved stereo

        // Decodes a .wav, .mp3, .flac or .ogg file to Audio's format (Core/Audio.cpp). Returns
        // false, with `samples` empty, if the file can't be read or isn't a format we know.
        bool DecodeFile(const std::string& path, std::vector<float>& samples);
    }

    // A decoded clip. The samples are shared so a playing voice keeps them alive even if the
    // asset is unloaded mid-play.
    struct Sound {
        std::shared_ptr<const std::vector<float>> samples;  // interleaved, Audio::Channels wide
        uint32_t frameCount = 0;
    };

}
