#include "Systems/AnimationSystem.h"

namespace Elysium::Systems {
void AnimationSystem::Update(float deltaTime)
{
    world->ForEachEntityWith<AnimationComponent, SpriteComponent>([&](Entity e) {
        auto& anim = world->GetComponent<AnimationComponent>(e);
        auto& sprite = world->GetComponent<SpriteComponent>(e);

        // Only animate if we have valid frame range
        if (anim.start <= anim.end && anim.frameDuration > 0) {
            anim.elapsed += deltaTime;
            
            if (anim.elapsed >= anim.frameDuration) {
                // Advance to next frame
                anim.currentFrame++;
                
                // Handle frame bounds
                if (anim.currentFrame > anim.end) {
                    if (anim.loop) {
                        anim.currentFrame = anim.start;
                    } else {
                        anim.currentFrame = anim.end; // Stay at last frame
                    }
                }
                
                // Update sprite component
                sprite.frameIndex = anim.currentFrame;
                anim.elapsed = 0;
            }
        }
    });
}
};
    