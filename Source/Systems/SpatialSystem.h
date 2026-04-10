#pragma once

#include "Core/System.h"
#include "Core/Component.h"
#include "raylib.h"
#include <vector>
#include <unordered_set>

namespace Elysium::Systems {

struct GridNode {
    int x, y;
    bool isWalkable = true;
    std::vector<Entity> occupants; // Dynamic entities in this cell
    
    // For A*
    float gCost = 0.0f;
    float hCost = 0.0f;
    GridNode* parent = nullptr;
    
    float fCost() const { return gCost + hCost; }
};

class SpatialSystem : public System {
public:
    SpatialSystem(Context context);
    
    void Update(float deltaTime) override;
    
    // Grid management
    void BuildGrid(int width, int height, float tileWidth, float tileHeight, bool isIsometric);
    GridNode* GetNode(int x, int y);
    GridNode* GetNodeFromWorld(Vector2 position);
    Vector2 GetWorldPosition(GridNode* node);
    
    // Pathfinding support
    std::vector<Vector2> FindPath(Vector2 start, Vector2 end);
    
    // Spatial queries
    std::vector<Entity> GetNearbyEntities(Vector2 position, float radius);
    bool IsWalkable(Vector2 position);

private:
    std::vector<GridNode> grid_;
    int gridWidth_ = 0;
    int gridHeight_ = 0;
    float tileWidth_ = 64.0f;
    float tileHeight_ = 32.0f;
    bool isIsometric_ = false;
    
    // A* Helpers
    float CalculateDistance(GridNode* a, GridNode* b);
    std::vector<GridNode*> GetNeighbors(GridNode* node);
};

} // namespace Elysium::Systems
