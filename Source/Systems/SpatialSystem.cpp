#include "Systems/SpatialSystem.h"
#include "Core/SystemRegistry.h"
#include "Core/Entity.h"
#include "Core/Scene.h"
#include "Core/Component.h"
#include "raymath.h"
#include <cfloat>
#include <algorithm>
#include <cmath>
#include "Components/TransformComponent.h"
#include "Components/BoundsComponent.h"

namespace Elysium::Systems {

SpatialSystem::SpatialSystem(Context context) : System(context) {
    // Grid is built from the tilemap in LoadScene via BuildGrid.
    // Nothing to do here.
}

void SpatialSystem::BuildGrid(int width, int height, float tileWidth, float tileHeight, bool isIsometric) {
    gridWidth_ = width;
    gridHeight_ = height;
    tileWidth_ = tileWidth;
    tileHeight_ = tileHeight;
    isIsometric_ = isIsometric;

    grid_.clear();
    grid_.resize(width * height);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            GridNode& node = grid_[y * width + x];
            node.x = x;
            node.y = y;
            node.isWalkable = true;
        }
    }
}

void SpatialSystem::Update(float deltaTime) {
    // 1. Reset dynamic occupants
    for (auto& node : grid_) {
        node.occupants.clear();
    }
    
    // 2. Re-bin all entities with Position + Bounds (or just Unit tag?)
    // Using BoundsComponent to determine "size" and Position for location
    world->Query<TransformComponent, BoundsComponent>([&](Entity e, auto& transform, auto& bounds) {
        GridNode* node = GetNodeFromWorld({transform.worldX, transform.worldY});
        if (node) {
            node->occupants.push_back(e);
        }
        
        // Mark static obstacles as unwalkable
        // Note: In a real game, we'd distinguish Static vs Dynamic better.
        // For now, let's assume if it has a TileComponent, it's static?
        // Or if it lacks a MovementComponent?
        // Let's assume everything is dynamic for now unless explicitly marked static.
        // We'll add logic later if needed.
    });
}

GridNode* SpatialSystem::GetNode(int x, int y) {
    if (x < 0 || x >= gridWidth_ || y < 0 || y >= gridHeight_) return nullptr;
    return &grid_[y * gridWidth_ + x];
}

GridNode* SpatialSystem::GetNodeFromWorld(Vector2 position) {
    int tileX, tileY;
    if (isIsometric_) {
        // Inverse of: worldX = (tileX - tileY) * (tileWidth/2)
        //             worldY = (tileX + tileY) * (tileHeight/2)
        float halfW = tileWidth_ * 0.5f;
        float halfH = tileHeight_ * 0.5f;
        float u = position.x / halfW;  // tileX - tileY
        float v = position.y / halfH;  // tileX + tileY
        // Round to nearest tile — same reason as Lua worldToTile: isometric tile containment
        // is the nearest tile center in tile-coordinate space, not a floor partition.
        tileX = static_cast<int>(std::round((u + v) * 0.5f));
        tileY = static_cast<int>(std::round((v - u) * 0.5f));
    } else {
        tileX = static_cast<int>(std::round(position.x / tileWidth_));
        tileY = static_cast<int>(std::round(position.y / tileHeight_));
    }
    return GetNode(tileX, tileY);
}

Vector2 SpatialSystem::GetWorldPosition(GridNode* node) {
    if (!node) return {0.0f, 0.0f};
    if (isIsometric_) {
        // Match SceneLoader tile placement: worldX = (tx-ty)*(tileWidth/2), worldY = (tx+ty)*(tileHeight/2)
        return Vector2{
            static_cast<float>((node->x - node->y)) * (tileWidth_  * 0.5f),
            static_cast<float>((node->x + node->y)) * (tileHeight_ * 0.5f)
        };
    }
    return Vector2{
        node->x * tileWidth_,
        node->y * tileHeight_
    };
}

std::vector<Entity> SpatialSystem::GetNearbyEntities(Vector2 position, float radius) {
    std::vector<Entity> result;
    
    GridNode* centerNode = GetNodeFromWorld(position);
    if (!centerNode) return result;
    
    // Check neighbor nodes roughly covering the radius
    // Use the smaller tile dimension so we don't under-cover narrow isometric cells.
    float minTileDim = std::min(tileWidth_, tileHeight_);
    int cellRange = static_cast<int>(std::ceil(radius / minTileDim));
    
    for (int y = centerNode->y - cellRange; y <= centerNode->y + cellRange; ++y) {
        for (int x = centerNode->x - cellRange; x <= centerNode->x + cellRange; ++x) {
            GridNode* node = GetNode(x, y);
            if (node) {
                for (Entity e : node->occupants) {
                    // Refine distance check
                    if (world->HasComponent<TransformComponent>(e)) {
                        auto& transform = world->GetComponent<TransformComponent>(e);
                        if (Vector2Distance(position, {transform.worldX, transform.worldY}) <= radius) {
                            result.push_back(e);
                        }
                    }
                }
            }
        }
    }
    return result;
}

bool SpatialSystem::IsWalkable(Vector2 position) {
    GridNode* node = GetNodeFromWorld(position);
    return node && node->isWalkable;
}

// --- Pathfinding Implementation (A*) ---

float SpatialSystem::CalculateDistance(GridNode* a, GridNode* b) {
    int dstX = std::abs(a->x - b->x);
    int dstY = std::abs(a->y - b->y);
    // Euclidean heuristic
    return std::sqrt(static_cast<float>(dstX * dstX + dstY * dstY));
}

std::vector<GridNode*> SpatialSystem::GetNeighbors(GridNode* node) {
    std::vector<GridNode*> neighbors;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            if (x == 0 && y == 0) continue;
            
            GridNode* n = GetNode(node->x + x, node->y + y);
            if (n && n->isWalkable) {
                neighbors.push_back(n);
            }
        }
    }
    return neighbors;
}

std::vector<Vector2> SpatialSystem::FindPath(Vector2 startPos, Vector2 endPos) {
    GridNode* startNode = GetNodeFromWorld(startPos);
    GridNode* endNode = GetNodeFromWorld(endPos);
    
    std::vector<Vector2> path;
    if (!startNode || !endNode || !endNode->isWalkable) {
        // Fallback: just go straight to target if valid
        if (endNode && endNode->isWalkable) path.push_back(endPos);
        return path;
    }
    
    if (startNode == endNode) {
        path.push_back(endPos);
        return path;
    }

    std::vector<GridNode*> openSet;
    std::unordered_set<GridNode*> closedSet;
    
    openSet.push_back(startNode);
    
    // Reset costs for this run (In a large system, use a runID to avoid clearing whole grid)
    // For this prototype, we'll assume low concurrency or just accept the cost.
    // Better: Use a "visited" map local to this function.
    // But GridNode stores gCost/hCost/parent. We must reset them or use a wrapper.
    // Let's use a wrapper map to avoid dirtying the global grid state.
    struct NodeRecord {
        GridNode* node;
        float gCost = 0;
        float hCost = 0;
        GridNode* parent = nullptr;
        
        NodeRecord() : node(nullptr), gCost(FLT_MAX), hCost(0), parent(nullptr) {}
        NodeRecord(GridNode* n, float g, float h, GridNode* p) : node(n), gCost(g), hCost(h), parent(p) {}
        
        float fCost() const { return gCost + hCost; }
    };
    
    // Using a map for sparse storage of node data during search
    // Key: GridNode index/pointer
    std::unordered_map<GridNode*, NodeRecord> nodeRecords;
    
    auto getRecord = [&](GridNode* n) -> NodeRecord& {
        if (nodeRecords.find(n) == nodeRecords.end()) {
            nodeRecords[n] = NodeRecord(n, FLT_MAX, 0, nullptr);
        }
        return nodeRecords[n];
    };
    
    NodeRecord& startRecord = getRecord(startNode);
    startRecord.gCost = 0;
    startRecord.hCost = CalculateDistance(startNode, endNode);
    
    while (!openSet.empty()) {
        // Find node with lowest fCost
        auto currentIt = openSet.begin();
        float lowestFCost = getRecord(*currentIt).fCost();
        
        for (auto it = openSet.begin(); it != openSet.end(); ++it) {
            if (getRecord(*it).fCost() < lowestFCost) {
                lowestFCost = getRecord(*it).fCost();
                currentIt = it;
            }
        }
        
        GridNode* current = *currentIt;
        openSet.erase(currentIt);
        closedSet.insert(current);
        
        if (current == endNode) {
            // Retrace path
            GridNode* curr = endNode;
            while (curr != startNode) {
                path.push_back(GetWorldPosition(curr));
                curr = getRecord(curr).parent;
            }
            std::reverse(path.begin(), path.end());
            return path;
        }
        
        for (GridNode* neighbor : GetNeighbors(current)) {
            if (closedSet.find(neighbor) != closedSet.end()) continue;
            
            float newGCost = getRecord(current).gCost + CalculateDistance(current, neighbor);
            NodeRecord& neighborRecord = getRecord(neighbor);
            
            bool inOpenSet = std::find(openSet.begin(), openSet.end(), neighbor) != openSet.end();
            
            if (newGCost < neighborRecord.gCost || !inOpenSet) {
                neighborRecord.gCost = newGCost;
                neighborRecord.hCost = CalculateDistance(neighbor, endNode);
                neighborRecord.parent = current;
                
                if (!inOpenSet) {
                    openSet.push_back(neighbor);
                }
            }
        }
    }
    
    // No path found
    return path;
}

}  // namespace Elysium::Systems

REGISTER_SYSTEM(Elysium::Systems::SpatialSystem)
