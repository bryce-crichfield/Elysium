#pragma once

#include "Core/System.h"
#include "Core/Entity.h"
#include <set>
#include <vector>

namespace Elysium::Systems {

// Unordered pair of entities - (A, B) == (B, A)
struct CollisionPair {
    Entity a;
    Entity b;

    CollisionPair(Entity e1, Entity e2) {
        // Always store in canonical order (smaller first)
        if (e1 < e2) {
            a = e1;
            b = e2;
        } else {
            a = e2;
            b = e1;
        }
    }

    bool operator<(const CollisionPair& other) const {
        if (a != other.a) return a < other.a;
        return b < other.b;
    }

    bool operator==(const CollisionPair& other) const {
        return a == other.a && b == other.b;
    }
};

class CollisionSystem : public System {
public:
    CollisionSystem(Context context);

    void Update(float deltaTime) override;

    // Access collision pairs from current frame
    const std::set<CollisionPair>& GetCollisions() const { return collisions_; }

    // Check if two specific entities are colliding
    bool AreColliding(Entity a, Entity b) const;

    // Get all entities colliding with a given entity
    std::vector<Entity> GetCollisionsWith(Entity entity) const;

private:
    std::set<CollisionPair> collisions_;
};

} // namespace Elysium::Systems
