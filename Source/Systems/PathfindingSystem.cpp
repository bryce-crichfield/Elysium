#include "Systems/PathfindingSystem.h"
#include "Systems/SpatialSystem.h"
#include "Core/Scene.h"
#include "Core/Component.h"
#include "Core/Entity.h"
#include "raymath.h"
#include "Components/PathRequestComponent.h"
#include "Components/PositionComponent.h"
#include "Components/MovementComponent.h"

namespace Elysium::Systems {

PathfindingSystem::PathfindingSystem(Context context) : System(context) {
}

void PathfindingSystem::Update(float deltaTime) {
    if (!spatialSystem_) {
        spatialSystem_ = scene->GetSystem<SpatialSystem>();
        if (!spatialSystem_) return; // Not ready or not added
    }

    // Process path requests
    // We use a manual iteration or collect entities first because we modify components (remove PathRequest)
    // which might invalidate iterators if not careful, but World::Query is safe for component removal?
    // World::Query iterates over living entities. Removing a component modifies the mask but doesn't shift the entity array immediately.
    // However, it's safer to collect entities then process.
    
    std::vector<Entity> requests;
    world->Query<PathRequestComponent, PositionComponent, MovementComponent>([&](Entity e, auto& req, auto& pos, auto& move) {
        requests.push_back(e);
    });
    
    for (Entity e : requests) {
        if (!world->HasComponent<PathRequestComponent>(e)) continue; // Double check
        
        auto& req = world->GetComponent<PathRequestComponent>(e);
        auto& pos = world->GetComponent<PositionComponent>(e);
        auto& move = world->GetComponent<MovementComponent>(e);
        
        // 1. Get Path from SpatialSystem
        // std::vector<Vector2> path = spatialSystem_->FindPath({pos.x, pos.y}, req.target);
        
        // 2. Update MovementComponent
            move.ClearWaypoints();
            move.AddWaypoint(req.target);
            move.isMoving = true;
        
        // 3. Remove request
        world->RemoveComponent<PathRequestComponent>(e);
    }
}

} // namespace Elysium::Systems
