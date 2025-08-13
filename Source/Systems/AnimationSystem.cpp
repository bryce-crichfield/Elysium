#include "Systems/AnimationSystem.h"

namespace Elysium::Systems {
void AnimationSystem::Update(float deltaTime)
{
    // Handle entities with directional animation
    world->Query<AnimationComponent, SpriteComponent, DirectionComponent>([&](Entity e, auto& anim, auto& sprite, auto& direction) {

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
                
                // Get new frame range and convert to relative indices
                auto frameRange = sprite.sprite.GetMarkerFrameRange(newMarker);
                anim.start = 0;  // Always start at relative frame 0
                anim.end = frameRange.second - frameRange.first;  // Convert to relative range
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
    world->Query<AnimationComponent, SpriteComponent>([&](Entity e, auto& anim, auto& sprite) {
        // Skip if entity also has DirectionComponent (handled above)
        if (world->HasComponent<DirectionComponent>(e)) return;

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
    