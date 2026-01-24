#pragma once

#include "Core/System.h"
#include "Core/Entity.h"

namespace Elysium::Systems {

class AttackSystem : public System {
public:
    AttackSystem(Context context);
    void Update(float deltaTime) override;

private:
    void SpawnProjectile(Entity attacker, const Vector2& startPos, Entity target, float damage);
};

} // namespace Elysium::Systems
