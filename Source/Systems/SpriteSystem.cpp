#include "Systems/SpriteSystem.h"
#include "Core/Component.h"
#include "Core/Entity.h"

namespace Elysium::Systems {

SpriteSystem::SpriteSystem(Context context) : System(context) {
}

void SpriteSystem::Update(float deltaTime) {
    // Update all sprite components with frame timing
    world->Query<SpriteComponent>([&](Entity entity, auto& spriteComp) {
        // Advance frame timing
        spriteComp.frameElapsed += deltaTime;

        // Check if it's time to advance to the next frame
        if (spriteComp.frameElapsed >= spriteComp.frameDuration) {
            spriteComp.frameElapsed -= spriteComp.frameDuration;  // Keep remainder for smooth timing

            // Get frame count for current marker
            int frameCount = spriteComp.sprite.GetMarkerFrameCount(spriteComp.markerName);
            if (frameCount > 0) {
                // Advance to next frame (loop back to 0 when reaching end)
                spriteComp.frameIndex = (spriteComp.frameIndex + 1) % frameCount;
            }
        }
    });
}

}  // namespace Elysium::Systems
