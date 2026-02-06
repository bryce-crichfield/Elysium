#include "Systems/SpatialSystem.h"
#include "Core/Entity.h"
#include "Core/Scene.h"
#include "Core/Component.h"
#include "raymath.h"
#include <cfloat>
#include <algorithm>
#include <cmath>
#include "Components/PositionComponent.h"
#include "Components/BoundsComponent.h"

namespace Elysium::Systems {

SpatialSystem::SpatialSystem(Context context) : System(context) {
    // Initialize a default grid size. In a real scenario, this matches the map size.
    BuildGrid(100, 100, 64);
}

void SpatialSystem::BuildGrid(int width, int height, int cellSize) {
    gridWidth_ = width;
    gridHeight_ = height;
    cellSize_ = cellSize;
    
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
    world->Query<PositionComponent, BoundsComponent>([&](Entity e, auto& pos, auto& bounds) {
        GridNode* node = GetNodeFromWorld({pos.x, pos.y});
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
    int x = static_cast<int>(position.x) / cellSize_;
    int y = static_cast<int>(position.y) / cellSize_;
    return GetNode(x, y);
}

Vector2 SpatialSystem::GetWorldPosition(GridNode* node) {
    if (!node) return {0, 0};
    return Vector2{
        static_cast<float>(node->x * cellSize_ + cellSize_ / 2),
        static_cast<float>(node->y * cellSize_ + cellSize_ / 2)
    };
}

std::vector<Entity> SpatialSystem::GetNearbyEntities(Vector2 position, float radius) {
    std::vector<Entity> result;
    
    GridNode* centerNode = GetNodeFromWorld(position);
    if (!centerNode) return result;
    
    // Check neighbor nodes roughly covering the radius
    int cellRange = static_cast<int>(std::ceil(radius / cellSize_));
    
    for (int y = centerNode->y - cellRange; y <= centerNode->y + cellRange; ++y) {
        for (int x = centerNode->x - cellRange; x <= centerNode->x + cellRange; ++x) {
            GridNode* node = GetNode(x, y);
            if (node) {
                for (Entity e : node->occupants) {
                    // Refine distance check
                    if (world->HasComponent<PositionComponent>(e)) {
                        auto& pos = world->GetComponent<PositionComponent>(e);
                        if (Vector2Distance(position, {pos.x, pos.y}) <= radius) {
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

} // namespace Elysium::Systems
