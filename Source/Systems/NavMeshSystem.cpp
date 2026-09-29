#include "Systems/NavMeshSystem.h"
#include "Systems/RenderSystem.h"
#include "Core/SystemRegistry.h"
#include "Core/Scene.h"
#include "Core/Geometry.h"
#include "Core/Log.h"
#include "Components/TransformComponent.h"
#include "Components/ColliderComponent.h"
#include "Components/KinematicsComponent.h"
#include "Components/MovementComponent.h"
#include "Components/NavAreaComponent.h"
#include <algorithm>
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
        {"isoRatio", Value{2.0f}},
        {"padding", Value{32.0f}},
        {"debugDraw", Value{true}},
    };
}

void NavMeshSystem::OnParametersChanged() {
    cellSize_    = std::max(2.0f, GetParameter("cellSize", 16.0f));
    agentRadius_ = std::max(0.0f, GetParameter("agentRadius", 10.0f));
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
        HashCombine(sig, HashFloat(collider.width));
        HashCombine(sig, HashFloat(collider.height));
        HashCombine(sig, HashString(collider.points));
    });
    return sig;
}

float NavMeshSystem::CarveRadius() const {
    // Half the cell diagonal, measured in the scaled space the distance metric uses, where a
    // cellSize square becomes cellSize x cellSize*isoRatio.
    const float halfDiagonal = 0.5f * cellSize_ * std::sqrt(1.0f + isoRatio_ * isoRatio_);
    return agentRadius_ + halfDiagonal;
}

NavMeshSystem::Inputs NavMeshSystem::GatherInputs() const {
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
        inputs.obstacles.push_back({polygon, Inflate(PolygonBounds(polygon), reach), 1.0f});
    });
    return inputs;
}

bool NavMeshSystem::AllocateGrid(const Inputs& inputs) {
    float minX = 1e30f, minY = 1e30f, maxX = -1e30f, maxY = -1e30f;
    auto grow = [&](const Area& a) {
        minX = std::min(minX, a.bounds.x);
        minY = std::min(minY, a.bounds.y);
        maxX = std::max(maxX, a.bounds.x + a.bounds.width);
        maxY = std::max(maxY, a.bounds.y + a.bounds.height);
    };

    const bool authoredRegion = !inputs.regions.empty();
    if (authoredRegion) {
        for (const auto& a : inputs.regions) grow(a);
    } else {
        for (const auto& a : inputs.obstacles) grow(a);
        for (const auto& a : inputs.blocked)   grow(a);
        for (const auto& a : inputs.costs)     grow(a);
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
    cells_.assign((size_t)width_ * height_, Cell{!authoredRegion, 1.0f});
    return true;
}

void NavMeshSystem::Rasterise(const Inputs& inputs) {
    auto forEachCellIn = [&](const Rectangle& b, auto&& fn) {
        int x0 = std::max(0, (int)std::floor((b.x - bounds_.x) / cellSize_));
        int y0 = std::max(0, (int)std::floor((b.y - bounds_.y) / cellSize_));
        int x1 = std::min(width_  - 1, (int)std::ceil((b.x + b.width  - bounds_.x) / cellSize_));
        int y1 = std::min(height_ - 1, (int)std::ceil((b.y + b.height - bounds_.y) / cellSize_));
        for (int cy = y0; cy <= y1; ++cy)
            for (int cx = x0; cx <= x1; ++cx) fn(CellCenter(cx, cy), cells_[IndexOf(cx, cy)]);
    };
    auto inside = [](Vector2 p, const Area& a) { return PointInPolygon(p, a.polygon); };

    for (const auto& a : inputs.regions)
        forEachCellIn(a.bounds, [&](Vector2 p, Cell& c) { if (inside(p, a)) c.walkable = true; });
    for (const auto& a : inputs.costs)
        forEachCellIn(a.bounds, [&](Vector2 p, Cell& c) { if (inside(p, a)) c.cost = std::max(c.cost, a.cost); });
    for (const auto& a : inputs.blocked)
        forEachCellIn(a.bounds, [&](Vector2 p, Cell& c) { if (inside(p, a)) c.walkable = false; });
    const float carveRadius = CarveRadius();
    for (const auto& o : inputs.obstacles)
        forEachCellIn(o.bounds, [&](Vector2 p, Cell& c) {
            if (c.walkable && DistanceToPolygon(p, o.polygon, AgentScale()) < carveRadius) c.walkable = false;
        });
}

void NavMeshSystem::Bake() {
    cells_.clear();
    width_ = height_ = 0;

    Inputs inputs = GatherInputs();
    if (!AllocateGrid(inputs)) return;
    Rasterise(inputs);

    size_t walkable = std::count_if(cells_.begin(), cells_.end(), [](const Cell& c) { return c.walkable; });
    LOG_INFOF("NavMesh", "Baked %dx%d @ %.0fpx (carve %.1f = agent %.1f + cell slack %.1f): %zu walkable, region=%s, %zu blocked, %zu cost, %zu obstacles",
              width_, height_, cellSize_, CarveRadius(), agentRadius_, CarveRadius() - agentRadius_,
              walkable, inputs.regions.empty() ? "auto" : "authored",
              inputs.blocked.size(), inputs.costs.size(), inputs.obstacles.size());
}

// --- Queries ----------------------------------------------------------------------------

const NavMeshSystem::Cell* NavMeshSystem::GetCell(int cx, int cy) const {
    if (cx < 0 || cy < 0 || cx >= width_ || cy >= height_) return nullptr;
    return &cells_[IndexOf(cx, cy)];
}

bool NavMeshSystem::WorldToCell(Vector2 p, int& cx, int& cy) const {
    if (width_ == 0) return false;
    cx = (int)std::floor((p.x - bounds_.x) / cellSize_);
    cy = (int)std::floor((p.y - bounds_.y) / cellSize_);
    return GetCell(cx, cy) != nullptr;
}

Vector2 NavMeshSystem::CellCenter(int cx, int cy) const {
    return {bounds_.x + (cx + 0.5f) * cellSize_, bounds_.y + (cy + 0.5f) * cellSize_};
}

bool NavMeshSystem::IsWalkable(Vector2 p) const {
    int cx, cy;
    return WorldToCell(p, cx, cy) && cells_[IndexOf(cx, cy)].walkable;
}

bool NavMeshSystem::NearestWalkable(Vector2 p, float maxDistance, Vector2& out) const {
    if (IsWalkable(p)) { out = p; return true; }
    if (width_ == 0) return false;

    int cx = (int)std::floor((p.x - bounds_.x) / cellSize_);
    int cy = (int)std::floor((p.y - bounds_.y) / cellSize_);
    int ring = (int)std::ceil(maxDistance / cellSize_);
    float best = maxDistance;
    bool found = false;
    for (int dy = -ring; dy <= ring; ++dy) {
        for (int dx = -ring; dx <= ring; ++dx) {
            const Cell* c = GetCell(cx + dx, cy + dy);
            if (!c || !c->walkable) continue;
            Vector2 center = CellCenter(cx + dx, cy + dy);
            float d = (center - p).Length();
            if (d <= best) { best = d; out = center; found = true; }
        }
    }
    return found;
}

bool NavMeshSystem::HasLineOfSight(Vector2 a, Vector2 b) const {
    int steps = std::max(1, (int)std::ceil((b - a).Length() / (cellSize_ * 0.5f)));
    for (int i = 0; i <= steps; ++i) {
        if (!IsWalkable(a + (b - a) * ((float)i / steps))) return false;
    }
    return true;
}

// --- Pathfinding ------------------------------------------------------------------------

std::vector<int> NavMeshSystem::AStar(int startIndex, int endIndex) const {
    struct Node { float f; int index; bool operator>(const Node& o) const { return f > o.f; } };
    const int ex = endIndex % width_, ey = endIndex / width_;
    auto heuristic = [&](int x, int y) {
        float dx = (float)std::abs(x - ex), dy = (float)std::abs(y - ey);
        return dx + dy + (kDiagonal - 2.0f) * std::min(dx, dy);
    };

    std::vector<float> g(cells_.size(), 1e30f);
    std::vector<int> parent(cells_.size(), -1);
    std::vector<bool> closed(cells_.size(), false);
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;

    g[startIndex] = 0.0f;
    open.push({heuristic(startIndex % width_, startIndex / width_), startIndex});

    while (!open.empty()) {
        int current = open.top().index;
        open.pop();
        if (closed[current]) continue;
        closed[current] = true;
        if (current == endIndex) break;

        int cx = current % width_, cy = current / width_;
        for (int k = 0; k < 8; ++k) {
            int nx = cx + kNeighbourDX[k], ny = cy + kNeighbourDY[k];
            const Cell* next = GetCell(nx, ny);
            if (!next || !next->walkable) continue;
            const bool diagonal = k >= 4;
            if (diagonal) {
                const Cell* a = GetCell(cx + kNeighbourDX[k], cy);
                const Cell* b = GetCell(cx, cy + kNeighbourDY[k]);
                if (!a || !b || !a->walkable || !b->walkable) continue;  // no corner cutting
            }
            int ni = IndexOf(nx, ny);
            if (closed[ni]) continue;
            float ng = g[current] + (diagonal ? kDiagonal : 1.0f) * next->cost;
            if (ng < g[ni]) {
                g[ni] = ng;
                parent[ni] = current;
                open.push({ng + heuristic(nx, ny), ni});
            }
        }
    }
    if (!closed[endIndex]) return {};

    std::vector<int> path;
    for (int i = endIndex; i != -1; i = parent[i]) path.push_back(i);
    std::reverse(path.begin(), path.end());
    return path;
}

std::vector<Vector2> NavMeshSystem::StringPull(const std::vector<Vector2>& raw) const {
    std::vector<Vector2> pulled;
    if (raw.empty()) return pulled;
    size_t anchor = 0;
    while (anchor < raw.size() - 1) {
        size_t farthest = anchor + 1;
        for (size_t j = raw.size() - 1; j > anchor + 1; --j) {
            if (HasLineOfSight(raw[anchor], raw[j])) { farthest = j; break; }
        }
        pulled.push_back(raw[farthest]);
        anchor = farthest;
    }
    return pulled;
}

std::vector<Vector2> NavMeshSystem::FindPath(Vector2 start, Vector2 end) const {
    if (width_ == 0) return {};

    Vector2 s, e;
    if (!NearestWalkable(start, cellSize_ * 4.0f, s)) return {};
    if (!NearestWalkable(end, cellSize_ * 6.0f, e)) return {};

    int sx, sy, ex, ey;
    WorldToCell(s, sx, sy);
    WorldToCell(e, ex, ey);
    if (sx == ex && sy == ey) return {e};

    std::vector<int> cells = AStar(IndexOf(sx, sy), IndexOf(ex, ey));
    if (cells.empty()) return {};

    std::vector<Vector2> raw;
    raw.reserve(cells.size());
    for (int i : cells) raw.push_back(CellCenter(i % width_, i / width_));
    raw.front() = s;
    raw.back() = e;
    return StringPull(raw);
}

// --- Movers -----------------------------------------------------------------------------

void NavMeshSystem::SlideMovers() {
    if (width_ == 0) return;
    world->Query<TransformComponent, KinematicsComponent, MovementComponent>([&](Entity e, auto& t, auto& kin, auto&) {
        Vector2 pos{t.worldX, t.worldY};
        if (IsWalkable(pos)) { lastWalkablePos_[e] = pos; return; }

        Vector2 last;
        if (auto it = lastWalkablePos_.find(e); it != lastWalkablePos_.end()) last = it->second;
        else if (!NearestWalkable(pos, cellSize_ * 4.0f, last)) return;

        // Keep whichever axis of this frame's motion stays on the mesh; cancel only the blocked one.
        Vector2 slideX{pos.x, last.y}, slideY{last.x, pos.y};
        bool okX = IsWalkable(slideX), okY = IsWalkable(slideY);
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
