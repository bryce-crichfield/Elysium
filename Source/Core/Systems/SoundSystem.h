#pragma once

#include <string>
#include <unordered_map>

#include "Core/Entity.h"
#include "Core/System.h"
#include "Interfaces/IAudioService.h"

namespace Elysium::Systems {

// Plays every SoundComponent through the audio service: starts its sound when `playing` is
// set, stops it when that's cleared, and clears `playing` when a sound that doesn't loop ends.
// Its sounds stop with their component, their entity, or the scene.
class SoundSystem : public System {
   public:
    using System::System;
    ~SoundSystem() override;
    void Update(float deltaTime) override;

   private:
    struct Voice {
        Services::SoundId id = Services::InvalidSound;
        std::string sound;  // what it's playing, to restart it when the component's changes
    };
    std::unordered_map<Entity, Voice> voices_;
};

}  // namespace Elysium::Systems
