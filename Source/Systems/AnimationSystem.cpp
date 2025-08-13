#include "Systems/AnimationSystem.h"

namespace Elysium::Systems {
void AnimationSystem::Update(float deltaTime)
{
    // Handle entities with directional animation
    world->ForEachEntityWith<AnimationComponent, SpriteComponent, DirectionComponent>([&](Entity e) {
        auto& anim = world->GetComponent<AnimationComponent>(e);
        auto& sprite = world->GetComponent<SpriteComponent>(e);
        auto& direction = world->GetComponent<DirectionComponent>(e);

        // Handle direction-based animation changes
        if (direction.hasChanged) {
            std::string baseAnimation;
            std::string directionSuffix;

            // Determine animation type based on direction
            if (direction.currentDirection == Direction::NONE) {
                baseAnimation = "idle";
                // Use last movement direction for idle pose
                switch (direction.previousDirection) {
                    case Direction::UP: directionSuffix = "up"; break;
                    case Direction::DOWN: directionSuffix = "down"; break;
                    case Direction::LEFT: directionSuffix = "left"; break;
                    case Direction::RIGHT: directionSuffix = "right"; break;
                    default: directionSuffix = "down"; break;
                }
            } else {
                baseAnimation = "walk";
                switch (direction.currentDirection) {
                    case Direction::UP: directionSuffix = "up"; break;
                    case Direction::DOWN: directionSuffix = "down"; break;
                    case Direction::LEFT: directionSuffix = "left"; break;
                    case Direction::RIGHT: directionSuffix = "right"; break;
                    default: directionSuffix = "down"; break;
                }
            }

            // Build new marker name
            std::string newMarker = baseAnimation + "/" + directionSuffix;
            
            // Switch animation if different
            if (anim.marker != newMarker) {
                anim.marker = newMarker;
                sprite.markerName = newMarker;
                
                // Get new frame range
                auto frameRange = sprite.sprite.GetMarkerFrameRange(newMarker);
                anim.start = frameRange.first;
                anim.end = frameRange.second;
                anim.currentFrame = anim.start;
                anim.elapsed = 0;
                sprite.frameIndex = anim.currentFrame;
            }
            
            direction.ClearChanged();
        }

        // Continue normal frame animation
        if (anim.start <= anim.end && anim.frameDuration > 0) {
            anim.elapsed += deltaTime;
            
            if (anim.elapsed >= anim.frameDuration) {
                anim.currentFrame++;
                
                if (anim.currentFrame > anim.end) {
                    if (anim.loop) {
                        anim.currentFrame = anim.start;
                    } else {
                        anim.currentFrame = anim.end;
                    }
                }
                
                sprite.frameIndex = anim.currentFrame;
                anim.elapsed = 0;
            }
        }
    });

    // Handle entities with animation but no direction (non-directional animation)
    world->ForEachEntityWith<AnimationComponent, SpriteComponent>([&](Entity e) {
        // Skip if entity also has DirectionComponent (handled above)
        if (world->HasComponent<DirectionComponent>(e)) return;
        
        auto& anim = world->GetComponent<AnimationComponent>(e);
        auto& sprite = world->GetComponent<SpriteComponent>(e);

        // Standard frame animation for non-directional entities
        if (anim.start <= anim.end && anim.frameDuration > 0) {
            anim.elapsed += deltaTime;
            
            if (anim.elapsed >= anim.frameDuration) {
                anim.currentFrame++;
                
                if (anim.currentFrame > anim.end) {
                    if (anim.loop) {
                        anim.currentFrame = anim.start;
                    } else {
                        anim.currentFrame = anim.end;
                    }
                }
                
                sprite.frameIndex = anim.currentFrame;
                anim.elapsed = 0;
            }
        }
    });
}
};
    