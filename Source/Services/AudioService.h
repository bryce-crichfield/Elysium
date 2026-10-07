#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
#include "Core/Audio.h"
#include "Core/ServiceLocator.h"
#include "Interfaces/IAudioService.h"

namespace Elysium::Services {

// Audio over PortAudio. PortAudio runs the device's callback on its own thread, and that
// thread is the mixer: it owns the playing voices and sums them into each buffer the device
// asks for. The main thread never touches the voices. Starting and stopping one goes through
// a command queue the callback picks up at the start of its next buffer; the callback only
// ever try-locks it, so a main thread holding it can delay a command by one buffer but never
// stall the device. Everything else about a playing sound (pause, loop, seek, the playhead)
// is a SoundState of atomics that both threads share.
class AudioService : public IAudioService {
   public:
    explicit AudioService(ServiceLocator& registry);
    ~AudioService() override;

    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;

    SoundId Play(const Path& asset, float volume = 1.0f, bool loop = false,
                 ChannelId channel = AudioChannel::Master) override;
    void Stop(SoundId id) override;
    void StopAll() override;

    void SetPaused(SoundId id, bool paused) override;
    void SetLooping(SoundId id, bool loop) override;
    void Seek(SoundId id, float seconds) override;
    std::optional<Playback> GetPlayback(SoundId id) const override;

    void SetChannelVolume(ChannelId channel, float volume) override;
    float GetChannelVolume(ChannelId channel) const override;

   private:
    // One sound's controls and playhead, shared by the main thread (sounds_) and the voice.
    struct SoundState {
        std::atomic<uint32_t> frameCount{0};  // 0 until its asset has loaded
        std::atomic<uint32_t> cursor{0};      // written by the audio thread
        std::atomic<int64_t> seekTo{-1};      // a frame to jump to, taken by the audio thread
        std::atomic<bool> paused{false};
        std::atomic<bool> loop{false};
        std::atomic<bool> finished{false};    // set by the audio thread when it drops the voice
    };

    struct Voice {
        SoundId id = InvalidSound;
        std::shared_ptr<const std::vector<float>> samples;
        std::shared_ptr<SoundState> state;
        uint32_t frameCount = 0;
        uint32_t cursor = 0;  // next frame to play
        float volume = 1.0f;
        ChannelId channel = AudioChannel::Master;
    };

    struct Command {
        enum class Kind { Play, Stop, StopAll } kind;
        Voice voice;  // Play: the voice to start; Stop: voice.id names the one to stop
    };

    void Mix(float* out, unsigned long frames);  // audio thread
    void Start(SoundId id, const Sound& sound, float volume, ChannelId channel);
    void Send(Command command);
    SoundState* Find(SoundId id) const;

    // How many sounds can play at once; past it, new ones are dropped. Reserved up front so
    // the callback never allocates.
    static constexpr size_t MaxVoices = 64;

    ServiceLocator& registry_;
    bool paInitialized_ = false;
    void* stream_ = nullptr;  // PaStream*; null if no device opened, and then nothing plays
    // Per channel, set by the main thread and read by the mixer each buffer.
    std::array<std::atomic<float>, AudioChannel::Count> channelVolumes_{};

    // Main thread -> audio thread.
    std::mutex commandMutex_;
    std::vector<Command> commands_;

    // Audio thread only. `received_` is swapped with `commands_` under the lock, so the
    // callback drains it unlocked; both keep their capacity across swaps.
    std::vector<Command> received_;
    std::vector<Voice> voices_;

    // Main thread only: every sound playing or loading, until Update sees it finish. A loading
    // sound erased from here was stopped, and doesn't start when its asset arrives.
    SoundId nextId_ = 1;
    std::unordered_map<SoundId, std::shared_ptr<SoundState>> sounds_;
};

}  // namespace Elysium::Services
