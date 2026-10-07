#pragma once
#include "Core/System.h"
#include "Core/Entity.h"
#include "Core/Math/Polygon.h"
#include "Core/Math/MathTypes.h"
#include "Core/Math/World3D.h"
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace Elysium::Systems {

// Where characters can walk: a navmesh baked from the scene's walkable models, carved by its
// static colliders. Without walkable models it's one flat floor around the colliders.
// Positions are ground positions (Transform x, y) plus a height (Transform z).
class NavigationSystem : public System {
public:
    NavigationSystem(Context context);
    ~NavigationSystem() override;

    void Update(float deltaTime) override;
    bool RunsWhenPaused() const override { return true; }

    // --- Queries ---------------------------------------------------------------------------

    bool IsWalkable(Vector2 worldPos) const;
    // Walkable within a step of height `z`.
    bool IsWalkable(Vector2 worldPos, float z) const;
    bool HasLineOfSight(Vector2 a, Vector2 b, float z = 0.0f) const;
    // `end` is a picture point (where a click lands), resolved to the floor drawn there. Waypoints
    // carry the floor's height, with one wherever it bends, so lerping between them follows it.
    std::vector<Vector3> FindPath(Vector2 start, Vector2 end, float startZ = 0.0f) const;
    std::optional<float> FloorHeight(Vector2 worldPos, float z = 0.0f) const;
    // Steps `from` by `delta` as far as the floor allows, sliding along walls.
    Vector3 Slide(Vector3 from, Vector2 delta) const;
    // The walkable floor drawn at picture point `p` in the default iso view.
    std::optional<Vector3> PickFloor(Vector2 p) const;
    std::optional<Vector3> PickFloor(const World3D::Ray& ray) const;
    // A sight line for attacks: no floor or static collider in the way.
    bool CanSee(Vector3 a, Vector3 b) const;
    float GroundDistance(Vector2 a, Vector2 b) const;

    // --- Reach -----------------------------------------------------------------------------

    // Everywhere a unit can walk within a ground-distance budget: a Dijkstra flood over a grid of
    // cellSize squares on the mesh. `blockers` are {x, y, radius} circles (other units). Only
    // valid until the next bake.
    struct Reach {
        struct Node {
            int cx, cy;
            float z;
            uint64_t poly;
            float cost;
            int parent;
        };
        uint64_t bake = 0;
        float budget = 0.0f;
        Vector2 origin{0, 0};
        float originZ = 0.0f;
        int start = -1;
        std::vector<Node> nodes;
        std::unordered_map<int64_t, std::vector<int>> cells;
        std::vector<Vector3> blockers;
    };
    // A row of reached cells on one floor, for drawing.
    struct ReachRun { float x0, x1, y, z, cost; };

    Reach ComputeReach(Vector2 start, float z, float budget, const std::vector<Vector3>& blockers) const;
    std::optional<float> ReachCost(const Reach& reach, Vector2 p, float z) const;
    std::vector<Vector3> ReachPath(const Reach& reach, Vector2 p, float z) const;
    std::vector<ReachRun> ReachRuns(const Reach& reach) const;

    // --- Debug -----------------------------------------------------------------------------

    std::vector<std::vector<Vector3>> Polygons() const;

protected:
    SystemParameters DefaultParameters() const override;
    void OnParametersChanged() override;

private:
    struct Area {
        Polygon polygon;
        Rectangle bounds;
        float low = -1e30f, high = 1e30f;
    };
    // In GL space.
    struct Inputs {
        std::vector<float> verts;
        std::vector<int> tris;
        std::vector<Area> obstacles;
    };
    struct Mesh;  // the baked mesh, defined in the .cpp
    using Ref = uint64_t;  // a mesh polygon

    // Bake.
    void Bake();
    Inputs GatherInputs();
    std::unique_ptr<Mesh> BuildMesh(const Inputs& inputs);
    uint64_t InputSignature() const;

    // Mesh helpers.
    Ref NearestPoly(Vector2 p, float z, float reach, float rise, Vector3* onMesh = nullptr) const;
    std::optional<float> HeightOn(Ref ref, Vector2 p) const;
    std::vector<float> FloorsAt(Vector2 p, float low, float high) const;
    void ForEachSurfaceTriangle(const std::function<void(const float*, const float*, const float*)>& visit) const;
    bool StraightWalk(Ref from, Vector2 a, Vector2 b, Ref* to = nullptr) const;
    // Appends a waypoint at each bend in the floor's height between `a` and `b`, then `b`.
    void AppendHeightBends(Vector3 a, Vector3 b, std::vector<Vector3>& out) const;

    // Movers.
    void SlideMovers();
    void DrawPaths();

    // Settings.
    float cellSize_ = 16.0f;
    float agentRadius_ = 10.0f;
    float agentHeight_ = 40.0f;
    float stepHeight_ = 24.0f;
    float padding_ = 32.0f;  // how far the flat floor reaches past the colliders
    bool debugDraw_ = true;

    // The mesh.
    std::unique_ptr<Mesh> mesh_;
    uint64_t lastSignature_ = 0;
    std::chrono::steady_clock::time_point changedAt_;
    bool stale_ = false;
    uint64_t bakeCount_ = 0;
    std::vector<Area> sightBlockers_;
    std::unordered_map<Entity, Vector2> lastWalkablePos_;
};

}  // namespace Elysium::Systems
