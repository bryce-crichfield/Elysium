#include "Systems/GroundSystem.h"

#include <vector>

#include "Components/ModelComponent.h"
#include "Components/MovementComponent.h"
#include "Components/TransformComponent.h"
#include "Core/Path.h"
#include "Core/SystemRegistry.h"
#include "Core/World3D.h"
#include "Interfaces/IAssetService.h"

namespace Elysium::Systems {

SystemParameters GroundSystem::DefaultParameters() const {
    return {
        {"stepHeight", Value{24.0f}},  // how far above its feet a unit finds the next surface
    };
}

void GroundSystem::Update(float /*deltaTime*/) {
    auto& assets = services->Get<Services::IAssetService>();
    const float stepHeight = GetParameter("stepHeight", 24.0f);

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

    world->Query<TransformComponent, MovementComponent>([&](Entity entity, auto& transform, auto&) {
        if (world->GetParent(entity) != INVALID_ENTITY) return;  // children ride on their parent
        const Vector3 from = World3D::ToGL(transform.localX, transform.localY, transform.localZ + stepHeight);
        float ground = 0.0f;
        for (const Surface& surface : surfaces) {
            if (auto height = World3D::SurfaceBelow(surface.matrix, *surface.model, from)) ground = std::max(ground, *height);
        }
        transform.localZ = ground;
    });
}

}  // namespace Elysium::Systems

REGISTER_SYSTEM(Elysium::Systems::GroundSystem)
