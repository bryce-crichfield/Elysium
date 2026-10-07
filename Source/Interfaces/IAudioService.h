#pragma once

#include <cstdint>
#include <optional>
#include "Core/Path.h"
#include "Interfaces/IService.h"

namespace Elysium::Services {

// A playing sound, as handed out by IAudioService::Play. Never reused; 0 is no sound.
using SoundId = uint32_t;
constexpr SoundId InvalidSound = 0;

// A mixer channel: every sound plays on one, each channel has its own volume, and every
// channel mixes into Master, whose volume scales them all. A sound played on Master itself
// is scaled by Master only.
using ChannelId = uint8_t;
namespace AudioChannel {
    constexpr ChannelId Master = 0;
    constexpr ChannelId Music = 1;
    constexpr ChannelId Effects = 2;
    constexpr ChannelId Ambient = 3;
    constexpr ChannelId Dialogue = 4;
    constexpr ChannelId Count = 5;
}

// Plays sound assets through the audio device. Sounds aren't tied to entities: a call plays
// one clip, fire-and-forget, and the returned SoundId is only needed to control it afterwards (stop
// a loop, or pause, seek and follow it, like the editor's sound preview does).
// The methods aren't named PlaySound/StopSound: <windows.h> (via enet) #defines PlaySound.
class IAudioService : public IService {
   public:
    // Where a sound is. Times are in seconds; `duration` is 0 while it's still loading.
    struct Playback {
        float position = 0.0f;
        float duration = 0.0f;
        bool paused = false;
        bool loop = false;
    };

    // `asset` is project-relative ("Sounds/Hit.wav"). A sound that isn't loaded yet is loaded
    // in the background and starts once it's ready. `volume` is the sound's own, under its
    // channel's (an unknown channel plays on Master). Never returns InvalidSound.
    virtual SoundId Play(const Path& asset, float volume = 1.0f, bool loop = false,
                         ChannelId channel = AudioChannel::Master) = 0;

    // Stops one sound (ids that already finished are ignored), or every sound.
    virtual void Stop(SoundId id) = 0;
    virtual void StopAll() = 0;

    // Control of one playing sound; ids that finished or were stopped are ignored. All of it
    // works while the sound is still loading, and applies once it starts.
    virtual void SetPaused(SoundId id, bool paused) = 0;
    virtual void SetLooping(SoundId id, bool loop) = 0;
    virtual void Seek(SoundId id, float seconds) = 0;
    // Null once the sound has finished or been stopped.
    virtual std::optional<Playback> GetPlayback(SoundId id) const = 0;

    // A channel's volume, 0..1; Master's scales everything. Unknown channels are ignored (0).
    virtual void SetChannelVolume(ChannelId channel, float volume) = 0;
    virtual float GetChannelVolume(ChannelId channel) const = 0;
};

}  // namespace Elysium::Services
