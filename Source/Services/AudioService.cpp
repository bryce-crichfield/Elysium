#include "Services/AudioService.h"

#include <algorithm>
#include "Core/Path.h"
#include "Interfaces/IAssetService.h"
#include "Services/LogService.h"
#include "portaudio.h"

namespace Elysium::Services {

AudioService::AudioService(ServiceLocator& registry) : registry_(registry) {
    for (auto& volume : channelVolumes_) volume.store(1.0f);
}

AudioService::~AudioService() { Shutdown(); }

// Opens `device` for stereo float output at the mixer's rate. Null if it refuses.
static PaStream* OpenOutput(PaDeviceIndex device, PaStreamCallback* callback, void* user) {
    if (device == paNoDevice) return nullptr;
    const PaDeviceInfo* info = Pa_GetDeviceInfo(device);
    if (!info || info->maxOutputChannels < Audio::Channels) return nullptr;

    PaStreamParameters params{};
    params.device = device;
    params.channelCount = Audio::Channels;
    params.sampleFormat = paFloat32;
    params.suggestedLatency = info->defaultLowOutputLatency;

    PaStream* stream = nullptr;
    const PaError err = Pa_OpenStream(&stream, nullptr, &params, Audio::SampleRate, paFramesPerBufferUnspecified,
                                      paNoFlag, callback, user);
    if (err != paNoError) {
        LOG_WARNINGF("AudioService", "Can't open %s: %s", info->name, Pa_GetErrorText(err));
        return nullptr;
    }
    LOG_INFOF("AudioService", "Audio output: %s (%s)", info->name, Pa_GetHostApiInfo(info->hostApi)->name);
    return stream;
}

void AudioService::Initialize() {
    commands_.reserve(MaxVoices);
    received_.reserve(MaxVoices);
    voices_.reserve(MaxVoices);

    const PaError err = Pa_Initialize();
    if (err != paNoError) {
        LOG_ERRORF("AudioService", "PortAudio failed to start, audio is off: %s", Pa_GetErrorText(err));
        return;
    }
    paInitialized_ = true;

    // WASAPI first where there is one: the default host API on Windows is MME, which adds a
    // lot of latency. WASAPI's shared mode only takes the device's own rate, so if that isn't
    // ours it refuses and the default device (which resamples) takes over.
    // Runs on PortAudio's thread (a lambda here, so it may call the private Mix).
    PaStreamCallback* callback = [](const void*, void* output, unsigned long frames, const PaStreamCallbackTimeInfo*,
                                    PaStreamCallbackFlags, void* user) -> int {
        static_cast<AudioService*>(user)->Mix(static_cast<float*>(output), frames);
        return paContinue;
    };
    PaStream* stream = nullptr;
    const PaHostApiIndex wasapi = Pa_HostApiTypeIdToHostApiIndex(paWASAPI);
    if (wasapi >= 0) stream = OpenOutput(Pa_GetHostApiInfo(wasapi)->defaultOutputDevice, callback, this);
    if (!stream) stream = OpenOutput(Pa_GetDefaultOutputDevice(), callback, this);
    if (!stream) {
        LOG_ERROR("AudioService", "No audio output device could be opened, audio is off");
        return;
    }

    const PaError startErr = Pa_StartStream(stream);
    if (startErr != paNoError) {
        LOG_ERRORF("AudioService", "Audio stream failed to start, audio is off: %s", Pa_GetErrorText(startErr));
        Pa_CloseStream(stream);
        return;
    }
    stream_ = stream;
}

void AudioService::Shutdown() {
    if (stream_) {
        PaStream* stream = static_cast<PaStream*>(stream_);
        Pa_StopStream(stream);  // waits for the callback to return; the voices are ours again
        Pa_CloseStream(stream);
        stream_ = nullptr;
    }
    if (paInitialized_) {
        Pa_Terminate();
        paInitialized_ = false;
    }
    voices_.clear();
    received_.clear();
    commands_.clear();
    sounds_.clear();
}

// Forgets the sounds the audio thread has finished with.
void AudioService::Update(float) {
    std::erase_if(sounds_, [](const auto& entry) { return entry.second->finished.load(); });
}

SoundId AudioService::Play(const Path& asset, float volume, bool loop, ChannelId channel) {
    if (channel >= AudioChannel::Count) channel = AudioChannel::Master;
    const SoundId id = nextId_++;
    if (!stream_) return id;

    auto state = std::make_shared<SoundState>();
    state->loop = loop;
    sounds_[id] = state;

    auto& assets = registry_.Get<IAssetService>();
    if (const Sound* sound = assets.Get<Sound>(asset)) {
        Start(id, *sound, volume, channel);
        return id;
    }

    // Not loaded: start it when it is, unless it was stopped in the meantime.
    assets.LoadAsset<Sound>(asset).Then([this, id, volume, channel, asset](IAsset* loaded) {
        if (!sounds_.count(id)) return;
        const Sound* sound = registry_.Get<IAssetService>().GetData<Sound>(loaded);
        if (!sound) {
            LOG_WARNINGF("AudioService", "Can't play %s: it didn't load", asset.c_str());
            sounds_.erase(id);
            return;
        }
        Start(id, *sound, volume, channel);
    });
    return id;
}

void AudioService::Stop(SoundId id) {
    auto it = sounds_.find(id);
    if (it == sounds_.end()) return;
    const bool started = it->second->frameCount.load() > 0;
    sounds_.erase(it);
    if (!started) return;  // still loading: its continuation sees it's gone
    Command command{Command::Kind::Stop, {}};
    command.voice.id = id;
    Send(std::move(command));
}

void AudioService::StopAll() {
    sounds_.clear();
    Send({Command::Kind::StopAll, {}});
}

AudioService::SoundState* AudioService::Find(SoundId id) const {
    auto it = sounds_.find(id);
    return it != sounds_.end() && !it->second->finished.load() ? it->second.get() : nullptr;
}

void AudioService::SetPaused(SoundId id, bool paused) {
    if (SoundState* state = Find(id)) state->paused = paused;
}

void AudioService::SetLooping(SoundId id, bool loop) {
    if (SoundState* state = Find(id)) state->loop = loop;
}

void AudioService::Seek(SoundId id, float seconds) {
    if (SoundState* state = Find(id)) state->seekTo = (int64_t)(std::max(0.0f, seconds) * Audio::SampleRate);
}

std::optional<IAudioService::Playback> AudioService::GetPlayback(SoundId id) const {
    const SoundState* state = Find(id);
    if (!state) return std::nullopt;
    // A seek the audio thread hasn't taken yet is already where the sound is, as far as the
    // caller is concerned (a scrub shouldn't snap back for a buffer).
    const int64_t pending = state->seekTo.load();
    const uint32_t frames = state->frameCount.load();
    const uint32_t cursor = pending >= 0 ? (uint32_t)std::min<int64_t>(pending, frames) : state->cursor.load();
    Playback playback;
    playback.position = (float)cursor / Audio::SampleRate;
    playback.duration = (float)frames / Audio::SampleRate;
    playback.paused = state->paused.load();
    playback.loop = state->loop.load();
    return playback;
}

void AudioService::SetChannelVolume(ChannelId channel, float volume) {
    if (channel < AudioChannel::Count) channelVolumes_[channel].store(std::clamp(volume, 0.0f, 1.0f));
}

float AudioService::GetChannelVolume(ChannelId channel) const {
    return channel < AudioChannel::Count ? channelVolumes_[channel].load() : 0.0f;
}

void AudioService::Start(SoundId id, const Sound& sound, float volume, ChannelId channel) {
    auto it = sounds_.find(id);
    if (it == sounds_.end()) return;
    if (!sound.samples || sound.frameCount == 0) {
        sounds_.erase(it);
        return;
    }
    it->second->frameCount = sound.frameCount;
    Voice voice;
    voice.id = id;
    voice.samples = sound.samples;
    voice.state = it->second;
    voice.frameCount = sound.frameCount;
    voice.volume = std::max(0.0f, volume);
    voice.channel = channel;
    Send({Command::Kind::Play, std::move(voice)});
}

void AudioService::Send(Command command) {
    if (!stream_) return;  // nobody would ever drain it
    std::lock_guard<std::mutex> lock(commandMutex_);
    commands_.push_back(std::move(command));
}

// --- Audio thread -------------------------------------------------------------------------

void AudioService::Mix(float* out, unsigned long frames) {
    if (commandMutex_.try_lock()) {
        received_.swap(commands_);
        commandMutex_.unlock();
    }
    auto drop = [](Voice& voice) { voice.state->finished = true; };
    for (Command& command : received_) {
        switch (command.kind) {
            case Command::Kind::Play:
                if (voices_.size() < MaxVoices) voices_.push_back(std::move(command.voice));
                else drop(command.voice);
                break;
            case Command::Kind::Stop:
                std::erase_if(voices_, [&](Voice& v) {
                    if (v.id != command.voice.id) return false;
                    drop(v);
                    return true;
                });
                break;
            case Command::Kind::StopAll:
                for (Voice& v : voices_) drop(v);
                voices_.clear();
                break;
        }
    }
    // Dropping a voice's samples here frees them only if their asset was unloaded meanwhile:
    // the asset holds the other reference.
    received_.clear();

    // Each channel's gain for this buffer; Master's is applied to the whole mix below.
    std::array<float, AudioChannel::Count> gains;
    for (ChannelId c = 0; c < AudioChannel::Count; c++) gains[c] = c == AudioChannel::Master ? 1.0f : channelVolumes_[c].load();

    std::fill(out, out + frames * Audio::Channels, 0.0f);
    for (size_t v = 0; v < voices_.size();) {
        Voice& voice = voices_[v];
        SoundState& state = *voice.state;
        if (const int64_t seek = state.seekTo.exchange(-1); seek >= 0) {
            voice.cursor = (uint32_t)std::min<int64_t>(seek, voice.frameCount);
        }

        bool done = false;
        if (!state.paused.load()) {
            const bool loop = state.loop.load();
            const float* samples = voice.samples->data();
            const float gain = voice.volume * gains[voice.channel];
            for (unsigned long f = 0; f < frames; f++) {
                if (voice.cursor >= voice.frameCount) {
                    if (!loop) { done = true; break; }
                    voice.cursor = 0;
                }
                out[f * 2] += samples[voice.cursor * 2] * gain;
                out[f * 2 + 1] += samples[voice.cursor * 2 + 1] * gain;
                voice.cursor++;
            }
        }
        state.cursor = voice.cursor;

        if (done) {
            drop(voice);
            if (v + 1 < voices_.size()) voices_[v] = std::move(voices_.back());
            voices_.pop_back();
        } else {
            v++;
        }
    }

    // Hard clip: a few loud sounds at once would otherwise wrap around in the device.
    const float master = channelVolumes_[AudioChannel::Master].load();
    for (unsigned long i = 0; i < frames * Audio::Channels; i++) {
        out[i] = std::clamp(out[i] * master, -1.0f, 1.0f);
    }
}

}  // namespace Elysium::Services
