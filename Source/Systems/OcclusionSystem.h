#pragma once
#include "Core/System.h"
#include "Core/Entity.h"
#include "Core/MathTypes.h"
#include "Core/Value.h"
#include <optional>
#include <unordered_map>
#include <vector>

namespace Elysium::Systems {

struct OccluderVolume {
    Entity entity = INVALID_ENTITY;
    Vector2 anchor;
    std::vector<Vector2> footprint;
    std::vector<Vector2> volume;
    Rectangle volumeBounds;
    float height = 0.0f;
    bool isStatic = true;
};

OccluderVolume ResolveOccluder(World& world, Entity entity);
bool IsBehind(const OccluderVolume& a, const OccluderVolume& b);

// Fades or tints Texture material layers when a dynamic occluder stands behind another.
// Not RunsWhenPaused: it mutates material overrides, and an editor save would persist them.
class OcclusionSystem : public System {
public:
    OcclusionSystem(Context context) : System(context) {}
    void Update(float deltaTime) override;

private:
    struct SavedTint {
        Entity entity;
        size_t layer;
        std::optional<Value> previous;
    };
    struct TintState {
        std::vector<SavedTint> saved;
        bool touched = false;
    };

    void ApplyTint(Entity root, Vector4 tint);
    void RestoreUntouched();

    std::unordered_map<Entity, TintState> tints_;
};

}  // namespace Elysium::Systems
