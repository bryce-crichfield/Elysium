#include "Systems/AnimationSystem.h"
#include "Entity.h" // For World
#include "raylib.h"     // For Vector2
#include <algorithm>
#include <variant>
#include "Animation.h"
#include "Scene.h"

namespace Elysium::Systems
{
AnimationSystem::AnimationSystem(Context context) : System(context)
{
}

void AnimationSystem::Update(float deltaTime)
{
    world->ForEachEntityWith<AnimationComponent>([&](Entity entity) {
        auto& animation = world->GetComponent<AnimationComponent>(entity);

        // Start next action if current is done
        if (!animation.currentAction && !animation.actionQueue.empty()) {
            animation.currentAction = animation.actionQueue.front();
            animation.actionQueue.pop();
        }

        // Process current action
        if (animation.currentAction && ProcessAction(*world, entity, animation.currentAction, deltaTime)) {
            animation.currentAction = nullptr;
        }
    });
}

void AnimationSystem::Render()
{
    // Animation system doesn't need rendering outside ImGui
}

void AnimationSystem::OnDraw()
{
}

void AnimationSystem::OnEvent(const char* eventName, void* data)
{
    // Animation system doesn't handle events
}

void AnimationSystem::DrawActionDetails(const Action& action, const std::string& id)
{
}

bool AnimationSystem::ProcessAction(World& world, Entity entity, std::shared_ptr<Action> action, float dt)
{
    if (!action) return true;

    return std::visit(
        [&](auto& act) -> bool {
            using T = std::decay_t<decltype(act)>;

            if constexpr (std::is_same_v<T, MoveTo>)
            {
                if (!world.HasComponent<PositionComponent>(entity) || !world.HasComponent<LocationComponent>(entity))
                {
                    return true;
                }

                PositionComponent& pos = world.GetComponent<PositionComponent>(entity);
                LocationComponent& loc = world.GetComponent<LocationComponent>(entity);

                if (!act.isStarted)
                {
                    act.startPos = Vector2{pos.x, pos.y};
                    // Convert target from tile coordinates to centered world coordinates
                    act.targetWorldPos = Vector2{
                        act.target.x * TILE_WIDTH + TILE_WIDTH * 0.5f,
                        act.target.y * TILE_HEIGHT + TILE_HEIGHT * 0.5f
                    };
                    act.isStarted = true;
                }
                act.elapsed += dt;
                float progress = std::min(act.elapsed / act.duration, 1.0f);

                pos.x = act.startPos.x + (act.targetWorldPos.x - act.startPos.x) * progress;
                pos.y = act.startPos.y + (act.targetWorldPos.y - act.startPos.y) * progress;

                // Update location component when movement completes
                if (progress >= 1.0f)
                {
                    loc.x = static_cast<int>(act.target.x);
                    loc.y = static_cast<int>(act.target.y);
                }

                return progress >= 1.0f;
            }
            else if constexpr (std::is_same_v<T, MoveBy>)
            {
                if (!world.HasComponent<PositionComponent>(entity) || !world.HasComponent<LocationComponent>(entity))
                {
                    return true;
                }

                PositionComponent& pos = world.GetComponent<PositionComponent>(entity);
                LocationComponent& loc = world.GetComponent<LocationComponent>(entity);

                if (!act.isStarted)
                {
                    act.startPos = Vector2{pos.x, pos.y};
                    act.isStarted = true;
                }
                act.elapsed += dt;
                float progress = std::min(act.elapsed / act.duration, 1.0f);

                pos.x = act.startPos.x + act.delta.x * progress;
                pos.y = act.startPos.y + act.delta.y * progress;

                // Update location component when movement completes
                if (progress >= 1.0f)
                {
                    loc.x = static_cast<int>((pos.x + TILE_WIDTH * 0.5f) / TILE_WIDTH);
                    loc.y = static_cast<int>((pos.y + TILE_HEIGHT * 0.5f) / TILE_HEIGHT);
                }

                return progress >= 1.0f;
            }
            else if constexpr (std::is_same_v<T, PlayFrames>)
            {
                if (!world.HasComponent<SpriteComponent>(entity))
                {
                    return true;
                }

                auto& sprite = world.GetComponent<SpriteComponent>(entity);
                act.elapsed += dt;
                float progress = act.elapsed / act.duration;
                int frameRange = act.end - act.start;
                act.currentFrame = act.start + static_cast<int>(progress * frameRange);
                // TODO: Update sprite marker based on currentFrame
                // For now, we'll skip updating the sprite

                return progress >= 1.0f;
            }
            else if constexpr (std::is_same_v<T, PlayMarker>)
            {
                if (!world.HasComponent<SpriteComponent>(entity))
                {
                    return true;
                }

                auto& spriteComponent = world.GetComponent<SpriteComponent>(entity);
                act.elapsed += dt;
                
                // Calculate frame timing
                int frameCount = spriteComponent.sprite.GetMarkerFrameCount(act.sheetName + "/" + act.markerName);
                if (frameCount == 0) {
                    return true; // Invalid marker
                }
                
                float totalDuration = act.frameDuration * frameCount;
                
                if (act.loop) {
                    // Loop the animation
                    while (act.elapsed >= totalDuration) {
                        act.elapsed -= totalDuration;
                    }
                }
                
                // Calculate current frame index
                int newFrameIndex = static_cast<int>(act.elapsed / act.frameDuration);
                newFrameIndex = std::min(newFrameIndex, frameCount - 1);
                
                // Update sprite component's frame index when frame changes
                if (newFrameIndex != act.currentFrameIndex) {
                    act.currentFrameIndex = newFrameIndex;
                    spriteComponent.frameIndex = newFrameIndex;
                }
                
                // Animation completes when we've gone through all frames and not looping
                if (!act.loop && act.elapsed >= totalDuration) {
                    return true;
                }
                
                return false; // Keep running if looping or not finished
            }
            else if constexpr (std::is_same_v<T, Wait>)
            {
                act.elapsed += dt;
                return act.elapsed >= act.duration;
            }
            else if constexpr (std::is_same_v<T, Callback>)
            {
                act.function();
                return true;
            }
            else if constexpr (std::is_same_v<T, Sequence>)
            {
                if (act.index >= act.actions.size())
                    return true;

                if (ProcessAction(world, entity, act.actions[act.index], dt))
                {
                    act.index++;
                }
                return act.index >= act.actions.size();
            }
            else if constexpr (std::is_same_v<T, Parallel>)
            {
                if (act.isCompletedFlags.empty())
                {
                    act.isCompletedFlags.resize(act.actions.size(), false);
                }

                bool allDone = true;
                for (size_t i = 0; i < act.actions.size(); ++i)
                {
                    if (!act.isCompletedFlags[i] && ProcessAction(world, entity, act.actions[i], dt))
                    {
                        act.isCompletedFlags[i] = true;
                    }
                    allDone &= act.isCompletedFlags[i];
                }
                return allDone;
            }
            else if constexpr (std::is_same_v<T, Repeat>)
            {
                if (act.count >= act.times)
                    return true;

                if (!act.current)
                {
                    act.current = CopyAction(act.actionTemplate);
                }

                if (ProcessAction(world, entity, act.current, dt))
                {
                    act.count++;
                    act.current = (act.count < act.times) ? CopyAction(act.actionTemplate) : nullptr;
                }

                return act.count >= act.times;
            }
            else if constexpr (std::is_same_v<T, Loop>)
            {
                if (!act.condition())
                    return true;

                if (!act.current)
                {
                    act.current = CopyAction(act.actionTemplate);
                }

                if (ProcessAction(world, entity, act.current, dt))
                {
                    act.current = CopyAction(act.actionTemplate);
                }

                return false;
            }

            return false;
        },
        action->data);
}

std::shared_ptr<Action> AnimationSystem::CopyAction(const std::shared_ptr<Action>& action)
{
    return std::visit(
        [&](const auto& act) -> std::shared_ptr<Action> {
            using T = std::decay_t<decltype(act)>;

            if constexpr (std::is_same_v<T, MoveTo>)
            {
                return std::make_shared<Action>(MoveTo{act.target, Vector2{}, Vector2{}, act.duration});
            }
            else if constexpr (std::is_same_v<T, MoveBy>)
            {
                return std::make_shared<Action>(MoveBy{act.delta, Vector2{}, act.duration});
            }
            else if constexpr (std::is_same_v<T, PlayFrames>)
            {
                return std::make_shared<Action>(PlayFrames{act.start, act.end, act.duration});
            }
            else if constexpr (std::is_same_v<T, PlayMarker>)
            {
                return std::make_shared<Action>(PlayMarker{act.sheetName, act.markerName, act.frameDuration, 0, 0, act.loop});
            }
            else if constexpr (std::is_same_v<T, Wait>)
            {
                return std::make_shared<Action>(Wait{act.duration});
            }
            else if constexpr (std::is_same_v<T, Callback>)
            {
                return std::make_shared<Action>(act);
            }
            else if constexpr (std::is_same_v<T, Sequence>)
            {
                std::vector<std::shared_ptr<Action>> copiedActions;
                for (auto& a : act.actions)
                {
                    copiedActions.push_back(CopyAction(a));
                }
                return std::make_shared<Action>(Sequence{copiedActions, 0});
            }
            else if constexpr (std::is_same_v<T, Parallel>)
            {
                std::vector<std::shared_ptr<Action>> copiedActions;
                for (auto& a : act.actions)
                {
                    copiedActions.push_back(CopyAction(a));
                }
                return std::make_shared<Action>(Parallel{copiedActions, std::vector<bool>()});
            }
            else if constexpr (std::is_same_v<T, Repeat>)
            {
                return std::make_shared<Action>(Repeat{act.times, 0, CopyAction(act.actionTemplate)});
            }
            else if constexpr (std::is_same_v<T, Loop>)
            {
                return std::make_shared<Action>(Loop{act.condition, CopyAction(act.actionTemplate)});
            }

            return nullptr;
        },
        action->data);
}

} // namespace Elysium
