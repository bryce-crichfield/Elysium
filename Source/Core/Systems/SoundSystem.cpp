#include "Core/Systems/SoundSystem.h"

#include <unordered_set>

#include "Core/Components/SoundComponent.h"
#include "Core/Path.h"
#include "Core/SystemRegistry.h"

namespace Elysium::Systems {

SoundSystem::~SoundSystem() {
    auto& audio = services->Get<Services::IAudioService>();
    for (const auto& [entity, voice] : voices_) audio.Stop(voice.id);
}

void SoundSystem::Update(float) {
    auto& audio = services->Get<Services::IAudioService>();
    
    std::unordered_set<Entity> seen;

    world->Query<SoundComponent>([&](Entity entity, SoundComponent& s) {
        seen.insert(entity);
        Voice& voice = voices_[entity];
        
        if (voice.id != Services::InvalidSound) {
            const bool ended = !audio.GetPlayback(voice.id);
            if (ended && !s.loop) s.playing = false;
            if (ended || !s.playing || voice.sound != s.sound) {
                audio.Stop(voice.id);
                voice = {};
            }
        }

        if (s.playing && voice.id == Services::InvalidSound && !s.sound.empty()) {
            voice = {
                audio.Play(Path(s.sound), 
                s.volume, 
                s.loop, 
                (Services::ChannelId)s.channel), 
                s.sound
            };
        }
    });

    std::erase_if(voices_, [&](const auto& entry) {
        if (seen.contains(entry.first)) return false;
        audio.Stop(entry.second.id);
        return true;
    });
}

}  // namespace Elysium::Systems

REGISTER_SYSTEM(Elysium::Systems::SoundSystem)
