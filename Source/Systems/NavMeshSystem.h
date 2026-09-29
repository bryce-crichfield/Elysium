#pragma once
#include "Core/System.h"
#include "Core/Entity.h"
#include "Core/MathTypes.h"
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Elysium::Systems {

// Scene-level walkability, baked into a cell grid from static colliders and painted
// NavAreas. Rebakes whenever those inputs change.
class NavMeshSystem : public System {
public:
    struct Cell {
        bool walkable = false;
        float cost = 1.0f;
    };

    NavMeshSystem(Context context) : System(context) {}

    void Update(float deltaTime) override;
    bool RunsWhenPaused() const override { return true; }

    bool IsWalkable(Vector2 worldPos) const;
    bool NearestWalkable(Vector2 worldPos, float maxDistance, Vector2& out) const;
    bool HasLineOfSight(Vector2 a, Vector2 b) const;
    std::vector<Vector2> FindPath(Vector2 start, Vector2 end) const;

    int Width() const { return width_; }
    int Height() const { return height_; }
    float CellSize() const { return cellSize_; }
    Rectangle Bounds() const { return bounds_; }
    const Cell* GetCell(int cx, int cy) const;
    Vector2 CellCenter(int cx, int cy) const;

protected:
    SystemParameters DefaultParameters() const override;
    void OnParametersChanged() override;

private:
    struct Area {
        std::vector<Vector2> polygon;
        Rectangle bounds;
        float cost = 1.0f;
    };
    struct Inputs {
        std::vector<Area> regions, blocked, costs, obstacles;
    };

    void Bake();
    Inputs GatherInputs() const;
    bool AllocateGrid(const Inputs& inputs);
    void Rasterise(const Inputs& inputs);
    uint64_t InputSignature() const;

    bool WorldToCell(Vector2 p, int& cx, int& cy) const;
    int IndexOf(int cx, int cy) const { return cy * width_ + cx; }
    std::vector<int> AStar(int startIndex, int endIndex) const;
    std::vector<Vector2> StringPull(const std::vector<Vector2>& path) const;

    void SlideMovers();
    void DrawPaths();

    // The space obstacle inflation is measured in: {1, isoRatio}. See AgentScale().
    Vector2 AgentScale() const { return {1.0f, isoRatio_}; }

    // What obstacles are actually inflated by. Cells are classified by their centre, but a mover
    // may stand anywhere in a walkable cell (IsWalkable is a cell lookup, and SlideMovers only
    // keeps it inside one), so carving by agentRadius alone leaves a cell whose far corner is up
    // to half a cell diagonal closer to the wall than the agent's radius allows. Collision then
    // pushes the agent out of ground the mesh called walkable — harmless in the open, a deadlock
    // in a channel where both walls do it at once. Adding the slack makes "walkable" mean "the
    // agent fits anywhere in this cell", which is what the path follower and HasLineOfSight assume.
    float CarveRadius() const;

    float cellSize_ = 16.0f;
    // Measured along x. Along y the carve reaches agentRadius_ / isoRatio_, so a ground-space
    // circle stays a circle instead of becoming an ellipse twice as deep as it should be.
    float agentRadius_ = 10.0f;
    // Screen tile width / height (64x32 iso = 2). 1 for a top-down scene.
    float isoRatio_ = 2.0f;
    float padding_ = 32.0f;
    bool debugDraw_ = true;

    std::vector<Cell> cells_;
    int width_ = 0, height_ = 0;
    Rectangle bounds_{0, 0, 0, 0};
    uint64_t lastSignature_ = 0;
    std::unordered_map<Entity, Vector2> lastWalkablePos_;
};

}  // namespace Elysium::Systems
