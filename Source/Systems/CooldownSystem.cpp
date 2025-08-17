#include "Systems/CooldownSystem.h"
#include "Scene.h"
#include "Entity.h"

#include "raylib.h"

namespace Elysium::Systems {
    void CooldownSystem::Update(float deltaTime) {
        world->Query<CooldownComponent>([&](Entity self, auto &cooldown)
        {
            if (!cooldown.isOnCooldown) { return; }

            cooldown.elapsedTime += deltaTime;
            if (cooldown.elapsedTime >= cooldown.cooldownTime)
            {
                cooldown.isOnCooldown = false;
                cooldown.elapsedTime = 0;
            }
        });
    }
};
