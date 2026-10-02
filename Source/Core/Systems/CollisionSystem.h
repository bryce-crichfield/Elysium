#pragma once

#include "Core/System.h"
#include "Core/Entity.h"
#include "Core/Math/MathTypes.h"
#include <map>
#include <set>
#include <vector>

namespace Elysium::Systems {

struct CollisionPair;

// How a confirmed overlap should be separated. `normal` is the world-space direction to push
// pair.a away from pair.b (b goes the other way) and `depth` is how far along it, so applying
// normal * depth exactly resolves the penetration.
//
// radial=false means the narrow phase fell through to the legacy box-vs-box path and left the
// separation to PhysicsResponseSystem's axis-aligned minimum-translation code. Circle paths set
// radial=true, which is the point of them: a radial normal varies continuously with the
// approach angle instead of snapping to x or y.
struct Contact {
    Vector2 normal{};
    float depth = 0.0f;
    bool radial = false;
};

// Collision systems job is to detect collisions.
// Typically it would run after movement systems and 
// before any systems that react to collisions (like damage or physics).
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

    // How this frame's overlap between a and b should be separated, or null if they aren't
    // touching. PhysicsResponseSystem reads this rather than recomputing the narrow phase.
    const Contact* GetContact(Entity a, Entity b) const;

protected:
    SystemParameters DefaultParameters() const override;
    void OnParametersChanged() override;

private:
    std::set<CollisionPair> collisions_;
    std::map<CollisionPair, Contact> contacts_;
    // Screen tile width / height (64x32 iso = 2), so a circle collider is a circle on the
    // ground rather than in screen pixels. 1 for a top-down scene.
    float isoRatio_ = 2.0f;
};

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

} // namespace Elysium::Systems
