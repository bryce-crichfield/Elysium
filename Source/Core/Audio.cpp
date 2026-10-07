#include "Core/Audio.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>

// The decoders are single-header libraries, compiled here: this is their only user.
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"
#define DR_FLAC_IMPLEMENTATION
#include "dr_flac.h"
#include "stb_vorbis.c"

namespace Elysium::Audio {

// What a decoder hands back: interleaved float frames at the file's own rate and width.
struct Decoded {
    std::vector<float> samples;
    int channels = 0;
    int sampleRate = 0;
};

template <typename T, typename Free>
static bool Take(T* data, uint64_t frames, int channels, int sampleRate, Free free, Decoded& out) {
    if (!data) return false;
    if (frames > 0 && channels > 0 && sampleRate > 0) {
        out.samples.assign(data, data + frames * channels);
        out.channels = channels;
        out.sampleRate = sampleRate;
    }
    free(data);
    return !out.samples.empty();
}

static bool DecodeWav(const char* path, Decoded& out) {
    unsigned int channels = 0, rate = 0;
    drwav_uint64 frames = 0;
    float* data = drwav_open_file_and_read_pcm_frames_f32(path, &channels, &rate, &frames, nullptr);
    return Take(data, frames, channels, rate, [](void* p) { drwav_free(p, nullptr); }, out);
}

static bool DecodeMp3(const char* path, Decoded& out) {
    drmp3_config config{};
    drmp3_uint64 frames = 0;
    float* data = drmp3_open_file_and_read_pcm_frames_f32(path, &config, &frames, nullptr);
    return Take(data, frames, config.channels, config.sampleRate, [](void* p) { drmp3_free(p, nullptr); }, out);
}

static bool DecodeFlac(const char* path, Decoded& out) {
    unsigned int channels = 0, rate = 0;
    drflac_uint64 frames = 0;
    float* data = drflac_open_file_and_read_pcm_frames_f32(path, &channels, &rate, &frames, nullptr);
    return Take(data, frames, channels, rate, [](void* p) { drflac_free(p, nullptr); }, out);
}

static bool DecodeOgg(const char* path, Decoded& out) {
    int channels = 0, rate = 0;
    short* data = nullptr;
    const int frames = stb_vorbis_decode_filename(path, &channels, &rate, &data);
    if (!data) return false;
    if (frames > 0 && channels > 0 && rate > 0) {
        out.samples.resize((size_t)frames * channels);
        for (size_t i = 0; i < out.samples.size(); i++) out.samples[i] = data[i] / 32768.0f;
        out.channels = channels;
        out.sampleRate = rate;
    }
    std::free(data);
    return !out.samples.empty();
}

// To Audio's format: mono is copied to both sides, past stereo only the first two channels
// are kept, then linear resampling to SampleRate. Good enough for effects; a proper filter
// can come with streaming music.
static std::vector<float> ToMixFormat(const Decoded& in) {
    const size_t inFrames = in.samples.size() / in.channels;
    auto at = [&](size_t frame, int side) {
        return in.samples[frame * in.channels + (in.channels == 1 ? 0 : side)];
    };

    if (in.sampleRate == SampleRate) {
        std::vector<float> out(inFrames * Channels);
        for (size_t f = 0; f < inFrames; f++) {
            out[f * 2] = at(f, 0);
            out[f * 2 + 1] = at(f, 1);
        }
        return out;
    }

    const double step = (double)in.sampleRate / SampleRate;
    const size_t outFrames = (size_t)(inFrames / step);
    std::vector<float> out(outFrames * Channels);
    for (size_t f = 0; f < outFrames; f++) {
        const double pos = f * step;
        const size_t i = (size_t)pos;
        const size_t j = std::min(i + 1, inFrames - 1);
        const float t = (float)(pos - i);
        for (int side = 0; side < Channels; side++) {
            out[f * 2 + side] = at(i, side) + (at(j, side) - at(i, side)) * t;
        }
    }
    return out;
}

bool DecodeFile(const std::string& path, std::vector<float>& samples) {
    samples.clear();
    std::string ext = std::filesystem::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });

    Decoded decoded;
    bool ok = false;
    if (ext == ".wav") ok = DecodeWav(path.c_str(), decoded);
    else if (ext == ".mp3") ok = DecodeMp3(path.c_str(), decoded);
    else if (ext == ".flac") ok = DecodeFlac(path.c_str(), decoded);
    else if (ext == ".ogg") ok = DecodeOgg(path.c_str(), decoded);
    if (!ok) return false;

    samples = ToMixFormat(decoded);
    return !samples.empty();
}

}  // namespace Elysium::Audio
