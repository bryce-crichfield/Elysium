#pragma once
#include "Core/System.h"
#include "Core/Entity.h"
#include "Core/Math/Polygon.h"
#include "Core/Math/MathTypes.h"
#include "Core/Math/World3D.h"
#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>
#include <vector>

namespace Elysium { struct Model; }

namespace Elysium::Systems {

// Scene-level walkability, baked into a layered grid: a column of ground cells, each holding
// every floor stacked over it. With walkable models (ModelComponent::walkable) the floors are
// their surfaces, found by casting down through each column, so a unit can stand under a bridge
// or on top of the stairs and the path knows which. Without any, it is one flat floor at height
// 0, made walkable by painted NavAreas. Static colliders carve it where their height range
// meets an agent's, Blocked and Cost NavAreas apply to every floor. Rebakes whenever an input
// changes.
//
// Positions are ground positions (Transform x, y) plus a height (Transform z). Neighbouring
// floors join when they're no more than stepHeight apart, which is what lets a path climb the
// stairs and not the wall beside them.
class NavMeshSystem : public System {
public:
    // One floor in a column.
    struct Cell {
        float z = 0.0f;
        bool walkable = false;
        float cost = 1.0f;
    };

    NavMeshSystem(Context context) : System(context) {}

    void Update(float deltaTime) override;
    bool RunsWhenPaused() const override { return true; }

    // Whether any floor at this ground position is walkable.
    bool IsWalkable(Vector2 worldPos) const;
    // Whether the floor a unit at height `z` stands on here is walkable.
    bool IsWalkable(Vector2 worldPos, float z) const;
    // Whether a unit at height `z` at `a` can walk straight to `b` (over floors that join).
    bool HasLineOfSight(Vector2 a, Vector2 b, float z = 0.0f) const;
    // A path from a unit at `start`, height `startZ`, to `end`. `end` is a point in the picture
    // (where a click lands, ground height 0): it goes to the floor actually drawn there, so
    // clicking the top of the stairs goes up them. Waypoints are ground positions (x, y) and
    // the height of the floor there (z), with one wherever the height along the way bends (the
    // foot and top of the stairs, not each step), so lerping between them follows the floor.
    std::vector<Vector3> FindPath(Vector2 start, Vector2 end, float startZ = 0.0f) const;

    // The height of the walkable floor a unit at height `z` would stand on at `worldPos` (the
    // nearest one within a couple of cells), or nothing.
    std::optional<float> FloorHeight(Vector2 worldPos, float z = 0.0f) const;
    // Steps a unit standing at `from` (ground x, y and the height z it stands at) by `delta`
    // on the ground, as far as the walkable floor lets it: blocked, it slides along the wall,
    // and it stays put if it can't move at all. z comes back as the height of the floor it
    // ends on. For steered movement (WASD), where FindPath is for walking to a point.
    Vector3 Slide(Vector3 from, Vector2 delta) const;
    // The walkable floor drawn at picture point `p` (where a click lands): its ground position
    // and height, or nothing.
    std::optional<Vector3> PickFloor(Vector2 p) const;
    // The same for any camera: the first walkable floor a view ray (GL, from the camera's
    // side; see World3D::View::RayAt) comes down onto.
    std::optional<Vector3> PickFloor(const World3D::Ray& ray) const;

    // Everywhere a unit can walk within a budget: a Dijkstra flood from where it stands, with
    // distance measured on the ground (a picture y counts isoRatio times an x, so a radius is
    // a circle on the ground, not on the screen). `blockers` are {x, y, radius} circles the
    // unit can't enter (other units). Only valid until the next bake; queries then return
    // nothing.
    struct Reach {
        uint64_t bake = 0;
        float budget = 0.0f;
        Vector2 origin{0, 0};
        int start = -1;
        std::vector<float> cost;       // per floor; > budget where unreached
        std::vector<int> parent;       // per floor
        std::vector<uint8_t> blocked;  // per column
        int minX = 0, minY = 0, maxX = -1, maxY = -1;  // the cells reached
    };
    // A horizontal run of reached cells on one floor height, for drawing: ground x from x0 to
    // x1, row centre y, height z, and the run's largest cost.
    struct ReachRun { float x0, x1, y, z, cost; };

    Reach ComputeReach(Vector2 start, float z, float budget, const std::vector<Vector3>& blockers) const;
    // The cost to walk to ground point `p` on the floor nearest height `z`, or nothing if it
    // isn't reached.
    std::optional<float> ReachCost(const Reach& reach, Vector2 p, float z) const;
    // The way from the reach's origin to `p`, as FindPath gives it (origin excluded), kept
    // clear of the blockers. Empty if `p` isn't reached.
    std::vector<Vector3> ReachPath(const Reach& reach, Vector2 p, float z) const;
    std::vector<ReachRun> ReachRuns(const Reach& reach) const;
    // Whether a straight sight line from `a` to `b` (ground x, y and an eye height z) is clear:
    // no floor rises above it and no static collider stands in it. For attacks, not walking.
    bool CanSee(Vector3 a, Vector3 b) const;
    // The ground distance between two ground points (the metric reach budgets are in).
    float GroundDistance(Vector2 a, Vector2 b) const;

    int Width() const { return width_; }
    int Height() const { return height_; }
    float CellSize() const { return cellSize_; }
    Rectangle Bounds() const { return bounds_; }
    // The column's top walkable floor (else its top floor), or nothing for an empty column.
    const Cell* GetCell(int cx, int cy) const;
    Vector2 CellCenter(int cx, int cy) const;

protected:
    SystemParameters DefaultParameters() const override;
    void OnParametersChanged() override;

private:
    struct Area {
        Polygon polygon;
        Rectangle bounds;
        float cost = 1.0f;
        float low = -1e30f, high = 1e30f;  // heights it occupies (obstacles)
    };
    struct Surface {
        Matrix matrix;
        const Model* model;
        Rectangle ground;  // its ground footprint
        float top = 0.0f;
    };
    struct Inputs {
        std::vector<Area> regions, blocked, costs, obstacles;
        std::vector<Surface> surfaces;
    };

    void Bake();
    Inputs GatherInputs();
    bool AllocateGrid(const Inputs& inputs);
    void BuildFloors(const Inputs& inputs);
    void Rasterise(const Inputs& inputs);
    uint64_t InputSignature() const;

    bool WorldToCell(Vector2 p, int& cx, int& cy) const;
    int IndexOf(int cx, int cy) const { return cy * width_ + cx; }
    // Floors are numbered across the whole grid; a column's run from columnStart_.
    int FloorCount(int column) const { return columnStart_[column + 1] - columnStart_[column]; }
    // The walkable floor of `column` that one at height `z` steps onto, or -1.
    int FloorNear(int column, float z) const;
    // The floor at `p` a unit at height `z` is on, else the nearest walkable one within
    // `maxDistance`; -1 if none.
    int NearestFloor(Vector2 p, float z, float maxDistance) const;
    // The floor drawn at picture point `p` (nearest the camera), or -1.
    int FloorInPicture(Vector2 p) const;
    // Walks from floor `from` at `a` toward `b`; the floor it ends on, or -1 if it can't.
    // `visit` sees each floor stepped onto, with where along the line it was entered.
    using WalkVisitor = std::function<void(Vector2 at, int floor)>;
    int Walk(int from, Vector2 a, Vector2 b, const WalkVisitor& visit = {}) const;
    // The waypoints from floor `from` at `a` to floor `to` at `b` (straight-walkable): one at
    // each bend in the floor's height along the way, then `b`.
    void AppendHeightBends(int from, Vector2 a, int to, Vector2 b, std::vector<Vector3>& out) const;
    std::vector<int> AStar(int start, int goal) const;

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
    // How tall an agent is: a floor needs this much room above it, and an obstacle carves the
    // floors it stands in this high.
    float agentHeight_ = 40.0f;
    // The biggest rise between neighbouring cells an agent walks up (a stair, not a wall).
    float stepHeight_ = 24.0f;
    // Screen tile width / height (64x32 iso = 2). 1 for a top-down scene.
    float isoRatio_ = 2.0f;
    float padding_ = 32.0f;
    bool debugDraw_ = true;

    std::vector<Cell> floors_;
    std::vector<int> floorColumn_;   // each floor's column
    std::vector<int> columnStart_;   // width*height + 1 entries
    int width_ = 0, height_ = 0;
    Rectangle bounds_{0, 0, 0, 0};
    float highestFloor_ = 0.0f;
    float lowestFloor_ = 0.0f;
    uint64_t lastSignature_ = 0;
    uint64_t bakeCount_ = 0;
    std::vector<Area> sightBlockers_;  // the static colliders, unpadded, from the last bake
    std::unordered_map<Entity, Vector2> lastWalkablePos_;
};

}  // namespace Elysium::Systems
