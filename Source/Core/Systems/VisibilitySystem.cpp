#include "Core/Systems/VisibilitySystem.h"
#include "Core/SystemRegistry.h"
#include "Core/Entity.h"
#include "Core/World.h"
#include "Core/World3D.h"
#include "Core/Components/TransformComponent.h"
#include "Core/Components/LayerComponent.h"
#include "Core/Components/LightComponent.h"
#include "Core/Components/ModelComponent.h"
#include <cmath>

namespace Elysium::Systems {

namespace {

// The fog of war's question, asked of the world instead of the picture: can any vision light
// (LightComponent::vision) see this point? Lighting answers it per pixel from the shadow maps;
// this answers it per unit by casting the same rays at the same models, so the two agree: in
// range (3D distance) and no model that casts shadows standing between them. Walkable models
// (floors, stairs) don't cast, so they don't block either.
struct Eye {
    Entity entity;
    Vector3 gl;
    float radius;
};

struct Blocker {
    Entity entity;
    Matrix matrix;
    const Model* model;
};

// How much of a vision radius counts as seen: Render3D's fog fades sight out over its last fifth.
constexpr float kSightReach = 0.9f;
// A unit is seen when the middle of it is, not its feet, which the floor's edge can hide.
constexpr float kTargetLift = 8.0f;

}  // namespace

void VisibilitySystem::Update(float) {
    std::vector<Eye> eyes;
    world->Query<TransformComponent, LightComponent>([&](Entity e, auto& transform, auto& light) {
        if (light.vision) {
            eyes.push_back({e, World3D::ToGL(transform.worldX, transform.worldY, transform.worldZ + light.height), light.radius});
        }
    });

    std::vector<Blocker> blockers;
    world->Query<TransformComponent, ModelComponent>([&](Entity e, auto& transform, auto& model) {
        if (model.walkable || !model.loaded || !model.loaded->native) return;
        blockers.push_back({e, World3D::ModelMatrix(transform, model, *model.loaded), model.loaded});
    });

    auto seen = [&](Entity target, Vector3 at) {
        for (const Eye& eye : eyes) {
            const Vector3 d{at.x - eye.gl.x, at.y - eye.gl.y, at.z - eye.gl.z};
            const float distance = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
            if (distance > eye.radius * kSightReach) continue;
            if (distance < 1e-3f) return true;
            const Vector3 direction{d.x / distance, d.y / distance, d.z / distance};
            bool blocked = false;
            for (const Blocker& b : blockers) {
                if (b.entity == eye.entity || b.entity == target) continue;
                const auto hit = World3D::RayDistance(b.matrix, *b.model, eye.gl, direction);
                if (hit && *hit < distance) { blocked = true; break; }
            }
            if (!blocked) return true;
        }
        return false;
    };

    // What hides in fog (LayerComponent::hideInFog) is hidden while no vision light sees it;
    // RenderSorter hides its children with it. With no vision lights there's no fog.
    world->Query<TransformComponent, LayerComponent>([&](Entity e, auto& transform, auto& layer) {
        layer.inFog = layer.hideInFog && !eyes.empty() &&
                      !seen(e, World3D::ToGL(transform.worldX, transform.worldY, transform.worldZ + kTargetLift));
    });
}

}  // namespace Elysium::Systems

REGISTER_SYSTEM(Elysium::Systems::VisibilitySystem)
