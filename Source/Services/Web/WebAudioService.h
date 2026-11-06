#pragma once

#include <array>
#include "Core/ServiceLocator.h"
#include "Interfaces/IAudioService.h"

namespace Elysium::Services {

// Silent audio for the browser until it gets a WebAudio backend. Sounds are handed ids so
// callers behave as on desktop, but nothing plays and every sound reads as already finished.
class WebAudioService : public IAudioService {
   public:
    explicit WebAudioService(ServiceLocator&) { channelVolumes_.fill(1.0f); }

    void Initialize() override {}
    void Shutdown() override {}
    void Update(float) override {}

    SoundId Play(const Path&, float = 1.0f, bool = false, ChannelId = AudioChannel::Master) override {
        return nextId_++;
    }
    void Stop(SoundId) override {}
    void StopAll() override {}

    void SetPaused(SoundId, bool) override {}
    void SetLooping(SoundId, bool) override {}
    void Seek(SoundId, float) override {}
    std::optional<Playback> GetPlayback(SoundId) const override { return std::nullopt; }

    void SetChannelVolume(ChannelId channel, float volume) override {
        if (channel < AudioChannel::Count) channelVolumes_[channel] = volume;
    }
    float GetChannelVolume(ChannelId channel) const override {
        return channel < AudioChannel::Count ? channelVolumes_[channel] : 0.0f;
    }

   private:
    SoundId nextId_ = 1;
    std::array<float, AudioChannel::Count> channelVolumes_{};
};

}  // namespace Elysium::Services
