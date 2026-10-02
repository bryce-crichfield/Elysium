#include "Systems/NavMeshSystem.h"
#include "Systems/RenderSystem.h"
#include "Core/SystemRegistry.h"
#include "Core/Scene.h"
#include "Core/Geometry.h"
#include "Core/Log.h"
#include "Core/Path.h"
#include "Core/World3D.h"
#include "Components/TransformComponent.h"
#include "Components/ColliderComponent.h"
#include "Components/KinematicsComponent.h"
#include "Components/ModelComponent.h"
#include "Components/MovementComponent.h"
#include "Components/NavAreaComponent.h"
#include "Interfaces/IAssetService.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <queue>
#include <string>

namespace Elysium::Systems {

namespace {

constexpr size_t kMaxCells = 4'000'000;
constexpr float kDiagonal = 1.41421356f;
constexpr int kNeighbourDX[8] = {1, -1, 0, 0, 1, 1, -1, -1};
constexpr int kNeighbourDY[8] = {0, 0, 1, -1, 1, -1, 1, -1};
// At most this many floors stacked in one column.
constexpr int kMaxFloors = 8;

void HashCombine(uint64_t& seed, uint64_t v) {
    seed ^= v + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
}
uint64_t HashFloat(float f) { return std::hash<int64_t>{}((int64_t)std::llround(f * 4.0f)); }
uint64_t HashString(const std::string& s) { return std::hash<std::string>{}(s); }

Rectangle Inflate(Rectangle r, Vector2 by) {
    return {r.x - by.x, r.y - by.y, r.width + 2 * by.x, r.height + 2 * by.y};
}

bool IsStaticObstacle(const World& world, Entity e, const ColliderComponent& collider) {
    return !collider.isTrigger && !world.HasComponent<KinematicsComponent>(e);
}

}  // namespace

SystemParameters NavMeshSystem::DefaultParameters() const {
    return {
        {"cellSize", Value{16.0f}},
        {"agentRadius", Value{10.0f}},
        {"agentHeight", Value{40.0f}},
        {"stepHeight", Value{24.0f}},
        {"isoRatio", Value{2.0f}},
        {"padding", Value{32.0f}},
        {"debugDraw", Value{true}},
    };
}

void NavMeshSystem::OnParametersChanged() {
    cellSize_    = std::max(2.0f, GetParameter("cellSize", 16.0f));
    agentRadius_ = std::max(0.0f, GetParameter("agentRadius", 10.0f));
    agentHeight_ = std::max(1.0f, GetParameter("agentHeight", 40.0f));
    stepHeight_  = std::max(0.0f, GetParameter("stepHeight", 24.0f));
    isoRatio_    = std::max(0.01f, GetParameter("isoRatio", 2.0f));
    padding_     = std::max(0.0f, GetParameter("padding", 32.0f));
    debugDraw_   = GetParameter("debugDraw", true);
    lastSignature_ = 0;
}

void NavMeshSystem::Update(float) {
    if (uint64_t signature = InputSignature(); signature != lastSignature_) {
        lastSignature_ = signature;
        Bake();
    }
    SlideMovers();
    if (debugDraw_) DrawPaths();
}

// --- Bake -------------------------------------------------------------------------------

uint64_t NavMeshSystem::InputSignature() const {
    uint64_t sig = 1469598103934665603ULL;
    HashCombine(sig, HashFloat(cellSize_));
    HashCombine(sig, HashFloat(agentRadius_));
    HashCombine(sig, HashFloat(agentHeight_));
    HashCombine(sig, HashFloat(stepHeight_));
    HashCombine(sig, HashFloat(isoRatio_));
    HashCombine(sig, HashFloat(padding_));

    World& w = *world;
    w.Query<TransformComponent, NavAreaComponent>([&](Entity e, auto& t, auto& area) {
        HashCombine(sig, e);
        HashCombine(sig, HashFloat(t.worldX));
        HashCombine(sig, HashFloat(t.worldY));
        HashCombine(sig, HashString(area.points));
        HashCombine(sig, HashString(area.type));
        HashCombine(sig, HashFloat(area.cost));
    });
    w.Query<TransformComponent, ColliderComponent>([&](Entity e, auto& t, auto& collider) {
        if (!IsStaticObstacle(w, e, collider)) return;
        HashCombine(sig, e);
        HashCombine(sig, HashFloat(t.worldX));
        HashCombine(sig, HashFloat(t.worldY));
        HashCombine(sig, HashFloat(t.worldZ));
        HashCombine(sig, HashFloat(collider.width));
        HashCombine(sig, HashFloat(collider.height));
        HashCombine(sig, HashFloat(collider.bottom));
        HashCombine(sig, HashFloat(collider.top));
        HashCombine(sig, HashString(collider.points));
    });
    auto& assets = services->Get<Services::IAssetService>();
    w.Query<TransformComponent, ModelComponent>([&](Entity e, auto& t, auto& model) {
        if (!model.walkable || model.modelPath.empty()) return;
        HashCombine(sig, e);
        HashCombine(sig, HashFloat(t.worldX));
        HashCombine(sig, HashFloat(t.worldY));
        HashCombine(sig, HashFloat(t.worldZ));
        HashCombine(sig, HashFloat(t.worldRotation));
        HashCombine(sig, HashFloat(t.worldScaleX * 100.0f));
        HashCombine(sig, HashFloat(t.worldScaleY * 100.0f));
        HashCombine(sig, HashFloat(model.scale * 100.0f));
        HashCombine(sig, HashFloat(model.yaw));
        HashCombine(sig, model.centered);
        HashCombine(sig, HashString(model.modelPath));
        // Rebake once the model has loaded: until then it has no surface to stand on.
        const Model* loaded = assets.Get<Model>(Path(model.modelPath));
        HashCombine(sig, loaded && loaded->native);
    });
    return sig;
}

float NavMeshSystem::CarveRadius() const {
    // Half the cell diagonal, measured in the scaled space the distance metric uses, where a
    // cellSize square becomes cellSize x cellSize*isoRatio.
    const float halfDiagonal = 0.5f * cellSize_ * std::sqrt(1.0f + isoRatio_ * isoRatio_);
    return agentRadius_ + halfDiagonal;
}

NavMeshSystem::Inputs NavMeshSystem::GatherInputs() {
    Inputs inputs;
    World& w = *world;

    w.Query<TransformComponent, NavAreaComponent>([&](Entity, auto& t, auto& area) {
        auto polygon = area.LocalPolygon();
        if (polygon.empty()) return;
        polygon = TranslatePolygon(polygon, {t.worldX, t.worldY});
        Area entry{polygon, PolygonBounds(polygon), std::max(1.0f, area.cost)};
        switch (area.Type()) {
            case NavAreaType::Walkable: inputs.regions.push_back(std::move(entry)); break;
            case NavAreaType::Blocked:  inputs.blocked.push_back(std::move(entry)); break;
            case NavAreaType::Cost:     inputs.costs.push_back(std::move(entry));   break;
        }
    });

    w.Query<TransformComponent, ColliderComponent>([&](Entity e, auto& t, auto& collider) {
        if (!IsStaticObstacle(w, e, collider)) return;
        auto polygon = collider.GetPolygon(t.worldX, t.worldY);
        // Scan bounds must cover exactly the carve reach, which is anisotropic like the metric.
        const float carve = CarveRadius();
        const Vector2 reach{carve, carve / isoRatio_};
        Area obstacle{polygon, Inflate(PolygonBounds(polygon), reach), 1.0f};
        collider.HeightRange(t.worldZ, obstacle.low, obstacle.high);
        inputs.obstacles.push_back(std::move(obstacle));
    });

    auto& assets = services->Get<Services::IAssetService>();
    w.Query<TransformComponent, ModelComponent>([&](Entity, auto& t, auto& component) {
        if (!component.walkable || component.modelPath.empty()) return;
        const Model* model = assets.Get<Model>(Path(component.modelPath));
        component.loaded = model;
        if (!model || !model->native) return;
        Surface surface{World3D::ModelMatrix(t, component, *model), model, {}, 0.0f};
        Vector3 lo, hi;
        World3D::ModelBounds(surface.matrix, *model, lo, hi);
        // GL (x, up, depth) back to the ground: depth is 2D y times kGroundDepth.
        surface.ground = {lo.x, lo.z / World3D::kGroundDepth, hi.x - lo.x, (hi.z - lo.z) / World3D::kGroundDepth};
        surface.top = hi.y;
        inputs.surfaces.push_back(surface);
    });
    return inputs;
}

bool NavMeshSystem::AllocateGrid(const Inputs& inputs) {
    float minX = 1e30f, minY = 1e30f, maxX = -1e30f, maxY = -1e30f;
    auto grow = [&](const Rectangle& b) {
        minX = std::min(minX, b.x);
        minY = std::min(minY, b.y);
        maxX = std::max(maxX, b.x + b.width);
        maxY = std::max(maxY, b.y + b.height);
    };

    if (!inputs.surfaces.empty()) {
        // The floors are the models: the grid covers them.
        for (const auto& s : inputs.surfaces) grow(s.ground);
    } else if (!inputs.regions.empty()) {
        for (const auto& a : inputs.regions) grow(a.bounds);
    } else {
        for (const auto& a : inputs.obstacles) grow(a.bounds);
        for (const auto& a : inputs.blocked)   grow(a.bounds);
        for (const auto& a : inputs.costs)     grow(a.bounds);
        minX -= padding_; minY -= padding_; maxX += padding_; maxY += padding_;
    }
    if (!(minX < maxX && minY < maxY)) return false;

    bounds_.x = std::floor(minX / cellSize_) * cellSize_;
    bounds_.y = std::floor(minY / cellSize_) * cellSize_;
    width_  = (int)std::ceil((maxX - bounds_.x) / cellSize_);
    height_ = (int)std::ceil((maxY - bounds_.y) / cellSize_);
    bounds_.width  = width_ * cellSize_;
    bounds_.height = height_ * cellSize_;

    if (width_ <= 0 || height_ <= 0 || (size_t)width_ * height_ > kMaxCells) {
        LOG_ERRORF("NavMesh", "Refusing to bake %dx%d cells; check cellSize and areas", width_, height_);
        width_ = height_ = 0;
        return false;
    }
    return true;
}

void NavMeshSystem::BuildFloors(const Inputs& inputs) {
    const int columns = width_ * height_;
    floors_.clear();
    floorColumn_.clear();
    columnStart_.assign(columns + 1, 0);
    highestFloor_ = 0.0f;

    if (inputs.surfaces.empty()) {
        // One flat floor everywhere, walkable unless painted regions say where it is.
        const bool walkable = inputs.regions.empty();
        floors_.assign(columns, Cell{0.0f, walkable, 1.0f});
        floorColumn_.resize(columns);
        for (int c = 0; c < columns; ++c) { columnStart_[c] = c; floorColumn_[c] = c; }
        columnStart_[columns] = columns;
        return;
    }

    // Every surface over each column, by rasterising the models' triangles onto the grid: each
    // column whose centre a triangle covers (seen from above) gets the triangle's height there.
    // Undersides come too (a slab's bottom); they have no room above them, which the headroom
    // test below drops. Walls (vertical triangles) cover no centre.
    std::vector<std::vector<float>> heights(columns);
    for (const Surface& surface : inputs.surfaces) {
        World3D::ForEachTriangle(surface.matrix, *surface.model, [&](Vector3 a, Vector3 b, Vector3 c) {
            // Ground positions (GL depth back to 2D y) and heights.
            const Vector2 pa{a.x, a.z / World3D::kGroundDepth}, pb{b.x, b.z / World3D::kGroundDepth}, pc{c.x, c.z / World3D::kGroundDepth};
            const float area = (pb.x - pa.x) * (pc.y - pa.y) - (pb.y - pa.y) * (pc.x - pa.x);
            if (std::fabs(area) < 1e-6f) return;
            const float minX = std::min({pa.x, pb.x, pc.x}), maxX = std::max({pa.x, pb.x, pc.x});
            const float minY = std::min({pa.y, pb.y, pc.y}), maxY = std::max({pa.y, pb.y, pc.y});
            const int x0 = std::max(0, (int)std::ceil((minX - bounds_.x) / cellSize_ - 0.5f));
            const int x1 = std::min(width_ - 1, (int)std::floor((maxX - bounds_.x) / cellSize_ - 0.5f));
            const int y0 = std::max(0, (int)std::ceil((minY - bounds_.y) / cellSize_ - 0.5f));
            const int y1 = std::min(height_ - 1, (int)std::floor((maxY - bounds_.y) / cellSize_ - 0.5f));
            for (int cy = y0; cy <= y1; ++cy) {
                for (int cx = x0; cx <= x1; ++cx) {
                    const Vector2 p = CellCenter(cx, cy);
                    // Barycentric weights of p; all on one side means inside.
                    const float wa = ((pb.x - p.x) * (pc.y - p.y) - (pb.y - p.y) * (pc.x - p.x)) / area;
                    const float wb = ((pc.x - p.x) * (pa.y - p.y) - (pc.y - p.y) * (pa.x - p.x)) / area;
                    const float wc = 1.0f - wa - wb;
                    if (wa < -1e-4f || wb < -1e-4f || wc < -1e-4f) continue;
                    heights[IndexOf(cx, cy)].push_back(wa * a.y + wb * b.y + wc * c.y);
                }
            }
        });
    }

    for (int column = 0; column < columns; ++column) {
        columnStart_[column] = (int)floors_.size();
        auto& h = heights[column];
        if (h.empty()) continue;
        std::sort(h.begin(), h.end(), std::greater<float>());
        float above = 1e30f;  // the surface over the last one kept or dropped
        int kept = 0;
        for (size_t i = 0; i < h.size() && kept < kMaxFloors; ++i) {
            if (i > 0 && h[i - 1] - h[i] < 0.5f) continue;  // the same surface (shared edges, coplanar faces)
            const float headroom = above - h[i];
            above = h[i];
            if (headroom < agentHeight_) continue;
            floors_.push_back({h[i], true, 1.0f});
            floorColumn_.push_back(column);
            highestFloor_ = std::max(highestFloor_, h[i]);
            ++kept;
        }
    }
    columnStart_[columns] = (int)floors_.size();
}

void NavMeshSystem::Rasterise(const Inputs& inputs) {
    auto forEachColumnIn = [&](const Rectangle& b, auto&& fn) {
        int x0 = std::max(0, (int)std::floor((b.x - bounds_.x) / cellSize_));
        int y0 = std::max(0, (int)std::floor((b.y - bounds_.y) / cellSize_));
        int x1 = std::min(width_  - 1, (int)std::ceil((b.x + b.width  - bounds_.x) / cellSize_));
        int y1 = std::min(height_ - 1, (int)std::ceil((b.y + b.height - bounds_.y) / cellSize_));
        for (int cy = y0; cy <= y1; ++cy)
            for (int cx = x0; cx <= x1; ++cx) {
                const int column = IndexOf(cx, cy);
                for (int f = columnStart_[column]; f < columnStart_[column + 1]; ++f) fn(CellCenter(cx, cy), floors_[f]);
            }
    };
    auto inside = [](Vector2 p, const Area& a) { return PointInPolygon(p, a.polygon); };

    // Painted walkable regions only matter on the flat floor; models say where their floors are.
    if (inputs.surfaces.empty()) {
        for (const auto& a : inputs.regions)
            forEachColumnIn(a.bounds, [&](Vector2 p, Cell& c) { if (inside(p, a)) c.walkable = true; });
    }
    for (const auto& a : inputs.costs)
        forEachColumnIn(a.bounds, [&](Vector2 p, Cell& c) { if (inside(p, a)) c.cost = std::max(c.cost, a.cost); });
    for (const auto& a : inputs.blocked)
        forEachColumnIn(a.bounds, [&](Vector2 p, Cell& c) { if (inside(p, a)) c.walkable = false; });
    const float carveRadius = CarveRadius();
    for (const auto& o : inputs.obstacles)
        forEachColumnIn(o.bounds, [&](Vector2 p, Cell& c) {
            // Only where it stands in an agent's way on this floor.
            if (o.high < c.z || o.low > c.z + agentHeight_) return;
            if (c.walkable && DistanceToPolygon(p, o.polygon, AgentScale()) < carveRadius) c.walkable = false;
        });
}

void NavMeshSystem::Bake() {
    const auto started = std::chrono::steady_clock::now();
    floors_.clear();
    floorColumn_.clear();
    columnStart_.clear();
    width_ = height_ = 0;

    Inputs inputs = GatherInputs();
    if (!AllocateGrid(inputs)) return;
    BuildFloors(inputs);
    Rasterise(inputs);

    const size_t walkable = std::count_if(floors_.begin(), floors_.end(), [](const Cell& c) { return c.walkable; });
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    LOG_INFOF("NavMesh", "Baked %dx%d @ %.0fpx in %.1f ms: %d floors, %d walkable, %d models",
              width_, height_, cellSize_, ms, (int)floors_.size(), (int)walkable, (int)inputs.surfaces.size());
}

// --- Queries ----------------------------------------------------------------------------

const NavMeshSystem::Cell* NavMeshSystem::GetCell(int cx, int cy) const {
    if (cx < 0 || cy < 0 || cx >= width_ || cy >= height_) return nullptr;
    const int column = IndexOf(cx, cy);
    const Cell* best = nullptr;
    for (int f = columnStart_[column]; f < columnStart_[column + 1]; ++f) {
        const Cell& c = floors_[f];
        if (!best || (c.walkable && !best->walkable) || (c.walkable == best->walkable && c.z > best->z)) best = &c;
    }
    return best;
}

bool NavMeshSystem::WorldToCell(Vector2 p, int& cx, int& cy) const {
    if (width_ == 0) return false;
    cx = (int)std::floor((p.x - bounds_.x) / cellSize_);
    cy = (int)std::floor((p.y - bounds_.y) / cellSize_);
    return cx >= 0 && cy >= 0 && cx < width_ && cy < height_;
}

Vector2 NavMeshSystem::CellCenter(int cx, int cy) const {
    return {bounds_.x + (cx + 0.5f) * cellSize_, bounds_.y + (cy + 0.5f) * cellSize_};
}

int NavMeshSystem::FloorNear(int column, float z) const {
    int best = -1;
    float bestRise = stepHeight_;
    for (int f = columnStart_[column]; f < columnStart_[column + 1]; ++f) {
        if (!floors_[f].walkable) continue;
        const float rise = std::fabs(floors_[f].z - z);
        if (rise <= bestRise) { bestRise = rise; best = f; }
    }
    return best;
}

bool NavMeshSystem::IsWalkable(Vector2 p) const {
    int cx, cy;
    if (!WorldToCell(p, cx, cy)) return false;
    const Cell* c = GetCell(cx, cy);
    return c && c->walkable;
}

bool NavMeshSystem::IsWalkable(Vector2 p, float z) const {
    int cx, cy;
    return WorldToCell(p, cx, cy) && FloorNear(IndexOf(cx, cy), z) >= 0;
}

int NavMeshSystem::NearestFloor(Vector2 p, float z, float maxDistance) const {
    if (width_ == 0) return -1;
    int cx = (int)std::floor((p.x - bounds_.x) / cellSize_);
    int cy = (int)std::floor((p.y - bounds_.y) / cellSize_);
    if (cx >= 0 && cy >= 0 && cx < width_ && cy < height_) {
        if (int f = FloorNear(IndexOf(cx, cy), z); f >= 0) return f;
    }
    const int ring = (int)std::ceil(maxDistance / cellSize_);
    float best = 1e30f;
    int found = -1;
    for (int dy = -ring; dy <= ring; ++dy) {
        for (int dx = -ring; dx <= ring; ++dx) {
            const int nx = cx + dx, ny = cy + dy;
            if (nx < 0 || ny < 0 || nx >= width_ || ny >= height_) continue;
            const float d = (CellCenter(nx, ny) - p).Length();
            if (d > maxDistance) continue;
            const int column = IndexOf(nx, ny);
            for (int f = columnStart_[column]; f < columnStart_[column + 1]; ++f) {
                if (!floors_[f].walkable) continue;
                const float score = d + std::fabs(floors_[f].z - z);
                if (score < best) { best = score; found = f; }
            }
        }
    }
    return found;
}

int NavMeshSystem::FloorInPicture(Vector2 p) const {
    // A floor at height z is drawn z * cos(pitch) higher than its ground position, so the floors
    // drawn at p stand at ground y = p.y + z * cos. Of those, the one nearest the camera.
    int cx, cy;
    if (!WorldToCell(p, cx, cy)) {
        cx = (int)std::floor((p.x - bounds_.x) / cellSize_);
        if (cx < 0 || cx >= width_) return -1;
    }
    const int y0 = (int)std::floor((p.y - bounds_.y) / cellSize_);
    const int y1 = (int)std::floor((p.y + highestFloor_ * World3D::kPitchCos - bounds_.y) / cellSize_) + 1;
    int best = -1;
    float bestDepth = -1e30f;
    for (int y = std::max(0, y0 - 1); y <= std::min(height_ - 1, y1); ++y) {
        const int column = IndexOf(cx, y);
        const float groundY = CellCenter(cx, y).y;
        for (int f = columnStart_[column]; f < columnStart_[column + 1]; ++f) {
            const Cell& c = floors_[f];
            if (!c.walkable) continue;
            if (std::fabs(groundY - c.z * World3D::kPitchCos - p.y) > cellSize_ * 0.5f) continue;
            const float depth = c.z * World3D::kPitchSin + groundY * World3D::kGroundDepth * World3D::kPitchCos;
            if (depth > bestDepth) { bestDepth = depth; best = f; }
        }
    }
    return best;
}

int NavMeshSystem::Walk(int from, Vector2 a, Vector2 b) const {
    if (from < 0) return -1;
    const int steps = std::max(1, (int)std::ceil((b - a).Length() / (cellSize_ * 0.5f)));
    int current = from;
    int column = floorColumn_[from];
    for (int i = 1; i <= steps; ++i) {
        const Vector2 p = a + (b - a) * ((float)i / steps);
        int cx, cy;
        if (!WorldToCell(p, cx, cy)) return -1;
        const int next = IndexOf(cx, cy);
        if (next == column) continue;
        const float z = floors_[current].z;
        // A diagonal step needs both sides open, as A* does: no cutting a corner.
        const int ox = column % width_, oy = column / width_;
        if (ox != cx && oy != cy) {
            if (FloorNear(IndexOf(cx, oy), z) < 0 || FloorNear(IndexOf(ox, cy), z) < 0) return -1;
        }
        current = FloorNear(next, z);
        if (current < 0) return -1;
        column = next;
    }
    return current;
}

bool NavMeshSystem::HasLineOfSight(Vector2 a, Vector2 b, float z) const {
    int cx, cy;
    if (!WorldToCell(a, cx, cy)) return false;
    return Walk(FloorNear(IndexOf(cx, cy), z), a, b) >= 0;
}

// --- Pathfinding ------------------------------------------------------------------------

std::vector<int> NavMeshSystem::AStar(int start, int goal) const {
    struct Node { float f; int index; bool operator>(const Node& o) const { return f > o.f; } };
    const int ex = floorColumn_[goal] % width_, ey = floorColumn_[goal] / width_;
    auto heuristic = [&](int x, int y) {
        float dx = (float)std::abs(x - ex), dy = (float)std::abs(y - ey);
        return dx + dy + (kDiagonal - 2.0f) * std::min(dx, dy);
    };

    std::vector<float> g(floors_.size(), 1e30f);
    std::vector<int> parent(floors_.size(), -1);
    std::vector<bool> closed(floors_.size(), false);
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;

    g[start] = 0.0f;
    open.push({heuristic(floorColumn_[start] % width_, floorColumn_[start] / width_), start});

    while (!open.empty()) {
        const int current = open.top().index;
        open.pop();
        if (closed[current]) continue;
        closed[current] = true;
        if (current == goal) break;

        const int cx = floorColumn_[current] % width_, cy = floorColumn_[current] / width_;
        const float z = floors_[current].z;
        for (int k = 0; k < 8; ++k) {
            const int nx = cx + kNeighbourDX[k], ny = cy + kNeighbourDY[k];
            if (nx < 0 || ny < 0 || nx >= width_ || ny >= height_) continue;
            const int next = FloorNear(IndexOf(nx, ny), z);
            if (next < 0 || closed[next]) continue;
            const bool diagonal = k >= 4;
            if (diagonal && (FloorNear(IndexOf(nx, cy), z) < 0 || FloorNear(IndexOf(cx, ny), z) < 0)) continue;  // no corner cutting
            const float ng = g[current] + (diagonal ? kDiagonal : 1.0f) * floors_[next].cost;
            if (ng < g[next]) {
                g[next] = ng;
                parent[next] = current;
                open.push({ng + heuristic(nx, ny), next});
            }
        }
    }
    if (!closed[goal]) return {};

    std::vector<int> path;
    for (int i = goal; i != -1; i = parent[i]) path.push_back(i);
    std::reverse(path.begin(), path.end());
    return path;
}

std::vector<Vector2> NavMeshSystem::FindPath(Vector2 start, Vector2 end, float startZ) const {
    if (width_ == 0) return {};

    const int s = NearestFloor(start, startZ, cellSize_ * 4.0f);
    int e = FloorInPicture(end);
    if (e < 0) e = NearestFloor(end, 0.0f, cellSize_ * 6.0f);
    if (s < 0 || e < 0) return {};

    // Where on the goal floor: the clicked point lifted to its height, when that's in the
    // goal's cell; else the cell's centre.
    auto centerOf = [&](int floor) { return CellCenter(floorColumn_[floor] % width_, floorColumn_[floor] / width_); };
    Vector2 goal = {end.x, end.y + floors_[e].z * World3D::kPitchCos};
    if (int gx, gy; !WorldToCell(goal, gx, gy) || IndexOf(gx, gy) != floorColumn_[e]) goal = centerOf(e);
    if (s == e) return {goal};

    const std::vector<int> nodes = AStar(s, e);
    if (nodes.empty()) return {};

    std::vector<Vector2> raw;
    raw.reserve(nodes.size());
    for (int f : nodes) raw.push_back(centerOf(f));
    if (int sx, sy; WorldToCell(start, sx, sy) && IndexOf(sx, sy) == floorColumn_[s]) raw.front() = start;
    raw.back() = goal;

    // String pulling: from each anchor, the farthest node it can walk straight to, ending on
    // that node's own floor (not the one above or below it).
    std::vector<Vector2> pulled;
    size_t anchor = 0;
    while (anchor < raw.size() - 1) {
        size_t farthest = anchor + 1;
        for (size_t j = raw.size() - 1; j > anchor + 1; --j) {
            if (Walk(nodes[anchor], raw[anchor], raw[j]) == nodes[j]) { farthest = j; break; }
        }
        pulled.push_back(raw[farthest]);
        anchor = farthest;
    }
    return pulled;
}

// --- Movers -----------------------------------------------------------------------------

void NavMeshSystem::SlideMovers() {
    if (width_ == 0) return;
    world->Query<TransformComponent, KinematicsComponent, MovementComponent>([&](Entity e, auto& t, auto& kin, auto&) {
        const Vector2 pos{t.worldX, t.worldY};
        const float z = t.worldZ;
        if (IsWalkable(pos, z)) { lastWalkablePos_[e] = pos; return; }

        Vector2 last;
        if (auto it = lastWalkablePos_.find(e); it != lastWalkablePos_.end()) {
            last = it->second;
        } else {
            const int f = NearestFloor(pos, z, cellSize_ * 4.0f);
            if (f < 0) return;
            last = CellCenter(floorColumn_[f] % width_, floorColumn_[f] / width_);
        }

        // Keep whichever axis of this frame's motion stays on the mesh; cancel only the blocked one.
        Vector2 slideX{pos.x, last.y}, slideY{last.x, pos.y};
        bool okX = IsWalkable(slideX, z), okY = IsWalkable(slideY, z);
        if (okX && okY) {
            bool preferX = std::fabs(pos.x - last.x) >= std::fabs(pos.y - last.y);
            okX = preferX;
            okY = !preferX;
        }
        Vector2 target = okX ? slideX : okY ? slideY : last;
        if (okX)      kin.velocity.y = 0.0f;
        else if (okY) kin.velocity.x = 0.0f;
        else          kin.velocity = {0, 0};

        t.localX += target.x - pos.x;
        t.localY += target.y - pos.y;
        t.worldX = target.x;
        t.worldY = target.y;
        lastWalkablePos_[e] = target;
    });
}

void NavMeshSystem::DrawPaths() {
    auto* render = scene->GetSystem<RenderSystem>();
    if (!render) return;
    const Color color{255, 255, 255, 200};
    world->Query<TransformComponent, MovementComponent>([&](Entity, auto& t, auto& mv) {
        Vector2 prev{t.worldX, t.worldY};
        for (int i = std::max(0, mv.currentWaypointIndex); i < (int)mv.waypoints.size(); ++i) {
            const Vector2& wp = mv.waypoints[i];
            render->IssueDrawCommand(DrawLineCmd{"selection", prev.x, prev.y, wp.x, wp.y, color});
            render->IssueDrawCommand(DrawCircleCmd{"selection", wp.x, wp.y, 3.0f, color});
            prev = wp;
        }
    });
}

}  // namespace Elysium::Systems

REGISTER_SYSTEM(Elysium::Systems::NavMeshSystem)
