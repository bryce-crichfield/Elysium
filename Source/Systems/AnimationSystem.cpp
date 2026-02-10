#include "Systems/AnimationSystem.h"
#include "Core/SystemRegistry.h"
#include "Components/AnimationComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/DirectionComponent.h"

namespace Elysium::Systems {
void AnimationSystem::Update(float deltaTime) {
}
}  // namespace Elysium::Systems

REGISTER_SYSTEM(Elysium::Systems::AnimationSystem)
