#include "Core/Systems/GroundSystem.h"

#include <cmath>
#include <vector>

#include "Core/Components/ModelComponent.h"
#include "Core/Components/MovementComponent.h"
#include "Core/Components/TransformComponent.h"
#include "Core/Path.h"
#include "Core/SystemRegistry.h"
#include "Core/Math/World3D.h"
#include "Interfaces/IAssetService.h"

namespace Elysium::Systems {

SystemParameters GroundSystem::DefaultParameters() const {
    return {
        {"stepHeight", Value{24.0f}},  // how far above its feet a unit finds the next surface
        {"heightEasing", Value{15.0f}},  // how fast a unit's height closes on its target, per second; 0 snaps
    };
}

void GroundSystem::Update(float deltaTime) {
    auto& assets = services->Get<Services::IAssetService>();
    const float stepHeight = GetParameter("stepHeight", 24.0f);
    const float easing = GetParameter("heightEasing", 15.0f);
    // The share of the gap closed this frame; all of it when paused, so an editor drag lands at once.
    const float blend = (easing > 0.0f && deltaTime > 0.0f) ? 1.0f - std::exp(-easing * deltaTime) : 1.0f;

    struct Surface {
        Matrix matrix;
        const Model* model;
    };
    std::vector<Surface> surfaces;
    world->Query<TransformComponent, ModelComponent>([&](Entity, auto& transform, auto& component) {
        if (!component.walkable || component.modelPath.empty()) return;
        const Model* model = assets.Get<Model>(Path(component.modelPath));
        component.loaded = model;
        if (!model || !model->native) return;
        surfaces.push_back({World3D::ModelMatrix(transform, component, *model), model});
    });

    world->Query<TransformComponent, MovementComponent>([&](Entity entity, auto& transform, auto& movement) {
        if (world->GetParent(entity) != INVALID_ENTITY) return;  // children ride on their parent
        const Vector3 from = World3D::ToGL(transform.localX, transform.localY, transform.localZ + stepHeight);
        float ground = 0.0f;
        for (const Surface& surface : surfaces) {
            if (auto height = World3D::SurfaceBelow(surface.matrix, *surface.model, from)) ground = std::max(ground, *height);
        }
        // On a path, the path's height: a smooth climb up stairs where the ground rises a step
        // at a time. Unless the path has left the ground (the unit was pushed off it, or the
        // floor changed under it), in which case the ground.
        float target = ground;
        if (auto onPath = movement.PathHeight({transform.localX, transform.localY})) {
            if (std::fabs(*onPath - ground) <= stepHeight) target = *onPath;
        }
        transform.localZ += (target - transform.localZ) * blend;
    });
}

}  // namespace Elysium::Systems

REGISTER_SYSTEM(Elysium::Systems::GroundSystem)
