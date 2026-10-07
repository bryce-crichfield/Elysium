#include "Core/Systems/NavigationSystem.h"
#include "Core/Systems/RenderSystem.h"
#include "Core/SystemRegistry.h"
#include "Core/Scene.h"
#include "Core/Math/Polygon.h"
#include "Core/Log.h"
#include "Core/Path.h"
#include "Core/Math/World3D.h"
#include "Core/Components/TransformComponent.h"
#include "Core/Components/ColliderComponent.h"
#include "Core/Components/KinematicsComponent.h"
#include "Core/Components/ModelComponent.h"
#include "Core/Components/MovementComponent.h"
#include "Interfaces/IAssetService.h"
#include <Recast.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshBuilder.h>
#include <DetourNavMeshQuery.h>
#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstring>
#include <map>
#include <queue>
#include <string>

namespace Elysium::Systems {

namespace {

constexpr float kGround = World3D::kGroundDepth;
constexpr size_t kMaxCells = 16'000'000;
constexpr float kMaxSlope = 50.0f;  // degrees
constexpr float kFar = 1e5f;
constexpr int kMaxPath = 1024;
constexpr int kMaxReachNodes = 200'000;
constexpr int kNeighbourDX[8] = {1, -1, 0, 0, 1, 1, -1, -1};
constexpr int kNeighbourDY[8] = {0, 0, 1, -1, 1, -1, 1, -1};

void HashCombine(uint64_t& seed, uint64_t v) {
    seed ^= v + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
}
uint64_t HashFloat(float f) { return std::hash<int64_t>{}((int64_t)std::llround(f * 4.0f)); }
uint64_t HashString(const std::string& s) { return std::hash<std::string>{}(s); }

bool IsStaticObstacle(const World& world, Entity e, const ColliderComponent& collider) {
    return !collider.isTrigger && !world.HasComponent<KinematicsComponent>(e);
}

void ToNav(Vector2 p, float z, float* out) { out[0] = p.x; out[1] = z; out[2] = p.y * kGround; }
Vector3 FromNav(const float* v) { return {v[0], v[2] / kGround, v[1]}; }

template <class T, void (*Free)(T*)>
struct RecastFree { void operator()(T* p) const { Free(p); } };
template <class T, void (*Free)(T*)>
using RecastPtr = std::unique_ptr<T, RecastFree<T, Free>>;

// Moller-Trumbore, two-sided.
std::optional<float> RayTriangle(const Vector3& o, const Vector3& d, const float* a, const float* b, const float* c) {
    const Vector3 e1{b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    const Vector3 e2{c[0] - a[0], c[1] - a[1], c[2] - a[2]};
    const Vector3 p{d.y * e2.z - d.z * e2.y, d.z * e2.x - d.x * e2.z, d.x * e2.y - d.y * e2.x};
    const float det = e1.x * p.x + e1.y * p.y + e1.z * p.z;
    if (std::fabs(det) < 1e-9f) return std::nullopt;
    const float inv = 1.0f / det;
    const Vector3 s{o.x - a[0], o.y - a[1], o.z - a[2]};
    const float u = (s.x * p.x + s.y * p.y + s.z * p.z) * inv;
    if (u < 0.0f || u > 1.0f) return std::nullopt;
    const Vector3 q{s.y * e1.z - s.z * e1.y, s.z * e1.x - s.x * e1.z, s.x * e1.y - s.y * e1.x};
    const float v = (d.x * q.x + d.y * q.y + d.z * q.z) * inv;
    if (v < 0.0f || u + v > 1.0f) return std::nullopt;
    const float t = (e2.x * q.x + e2.y * q.y + e2.z * q.z) * inv;
    if (t <= 0.0f) return std::nullopt;
    return t;
}

}  // namespace

struct NavigationSystem::Mesh {
    RecastPtr<dtNavMesh, dtFreeNavMesh> mesh;
    RecastPtr<dtNavMeshQuery, dtFreeNavMeshQuery> query;
    dtQueryFilter filter;
};

NavigationSystem::NavigationSystem(Context context) : System(context) {}
NavigationSystem::~NavigationSystem() = default;

SystemParameters NavigationSystem::DefaultParameters() const {
    return {
        {"cellSize", Value{16.0f}},
        {"agentRadius", Value{10.0f}},
        {"agentHeight", Value{40.0f}},
        {"stepHeight", Value{24.0f}},
        {"padding", Value{32.0f}},
        {"debugDraw", Value{true}},
    };
}

void NavigationSystem::OnParametersChanged() {
    cellSize_    = std::max(1.0f, GetParameter("cellSize", 16.0f));
    agentRadius_ = std::max(0.0f, GetParameter("agentRadius", 10.0f));
    agentHeight_ = std::max(1.0f, GetParameter("agentHeight", 40.0f));
    stepHeight_  = std::max(0.0f, GetParameter("stepHeight", 24.0f));
    padding_     = std::max(0.0f, GetParameter("padding", 32.0f));
    debugDraw_   = GetParameter("debugDraw", true);
    lastSignature_ = 0;
}

void NavigationSystem::Update(float) {
    // Bakes are slow, so wait for an editor drag to settle before rebaking.
    constexpr auto kSettle = std::chrono::milliseconds(300);
    const auto now = std::chrono::steady_clock::now();
    if (uint64_t signature = InputSignature(); signature != lastSignature_) {
        lastSignature_ = signature;
        changedAt_ = now;
        stale_ = true;
    }
    if (stale_ && (bakeCount_ == 0 || now - changedAt_ >= kSettle)) {
        stale_ = false;
        Bake();
    }
    SlideMovers();
    if (debugDraw_) DrawPaths();
}

// --- Bake -------------------------------------------------------------------------------

uint64_t NavigationSystem::InputSignature() const {
    uint64_t sig = 1469598103934665603ULL;
    HashCombine(sig, HashFloat(cellSize_));
    HashCombine(sig, HashFloat(agentRadius_));
    HashCombine(sig, HashFloat(agentHeight_));
    HashCombine(sig, HashFloat(stepHeight_));
    HashCombine(sig, HashFloat(padding_));

    World& w = *world;
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
        const Model* loaded = assets.Get<Model>(Path(model.modelPath));
        HashCombine(sig, loaded && loaded->native);
    });
    return sig;
}

NavigationSystem::Inputs NavigationSystem::GatherInputs() {
    Inputs inputs;
    World& w = *world;
    auto addTriangle = [&](Vector3 a, Vector3 b, Vector3 c) {
        const int base = (int)inputs.verts.size() / 3;
        for (const Vector3& v : {a, b, c}) inputs.verts.insert(inputs.verts.end(), {v.x, v.y, v.z});
        inputs.tris.insert(inputs.tris.end(), {base, base + 1, base + 2});
    };
    auto addFlat = [&](const Polygon& polygon) {
        const std::vector<Vector2> corners = polygon.Triangulate();
        for (size_t i = 0; i + 2 < corners.size(); i += 3) {
            addTriangle(World3D::ToGL(corners[i].x, corners[i].y, 0.0f), World3D::ToGL(corners[i + 1].x, corners[i + 1].y, 0.0f),
                        World3D::ToGL(corners[i + 2].x, corners[i + 2].y, 0.0f));
        }
    };

    w.Query<TransformComponent, ColliderComponent>([&](Entity e, auto& t, auto& collider) {
        if (!IsStaticObstacle(w, e, collider)) return;
        Polygon polygon = collider.GetPolygon(t.worldX, t.worldY);
        Area obstacle{polygon, polygon.Bounds()};
        collider.HeightRange(t.worldZ, obstacle.low, obstacle.high);
        inputs.obstacles.push_back(std::move(obstacle));
    });

    auto& assets = services->Get<Services::IAssetService>();
    w.Query<TransformComponent, ModelComponent>([&](Entity, auto& t, auto& component) {
        if (!component.walkable || component.modelPath.empty()) return;
        const Model* model = assets.Get<Model>(Path(component.modelPath));
        component.loaded = model;
        if (!model || !model->native) return;
        World3D::ForEachTriangle(World3D::ModelMatrix(t, component, *model), *model, addTriangle);
    });

    if (inputs.tris.empty()) {
        float minX = 1e30f, minY = 1e30f, maxX = -1e30f, maxY = -1e30f;
        for (const Area& o : inputs.obstacles) {
            minX = std::min(minX, o.bounds.x);
            minY = std::min(minY, o.bounds.y);
            maxX = std::max(maxX, o.bounds.x + o.bounds.width);
            maxY = std::max(maxY, o.bounds.y + o.bounds.height);
        }
        if (minX < maxX && minY < maxY) {
            addFlat(Polygon::FromRectangle({minX - padding_, minY - padding_, maxX - minX + 2 * padding_,
                                            maxY - minY + 2 * padding_}));
        }
    }
    return inputs;
}

std::unique_ptr<NavigationSystem::Mesh> NavigationSystem::BuildMesh(const Inputs& inputs) {
    const int vertCount = (int)inputs.verts.size() / 3;
    const int triCount = (int)inputs.tris.size() / 3;
    rcContext ctx(false);

    rcConfig cfg{};
    cfg.cs = cellSize_;
    // A step spans several cells, so stairs read as climbs rather than walls.
    cfg.ch = std::clamp(cellSize_ * 0.5f, 0.1f, std::max(0.1f, stepHeight_ / 4.0f));
    cfg.walkableSlopeAngle = kMaxSlope;
    cfg.walkableHeight = (int)std::ceil(agentHeight_ / cfg.ch);
    cfg.walkableClimb = (int)std::floor(stepHeight_ / cfg.ch);
    cfg.walkableRadius = (int)std::ceil(agentRadius_ / cfg.cs);
    cfg.maxEdgeLen = (int)std::max(8.0f, agentRadius_ * 8.0f / cfg.cs);
    cfg.maxSimplificationError = 1.3f;
    cfg.minRegionArea = 8 * 8;
    cfg.mergeRegionArea = 20 * 20;
    cfg.maxVertsPerPoly = DT_VERTS_PER_POLYGON;
    cfg.detailSampleDist = cfg.cs * 6.0f;
    cfg.detailSampleMaxError = cfg.ch;
    rcCalcBounds(inputs.verts.data(), vertCount, cfg.bmin, cfg.bmax);
    cfg.bmax[1] += agentHeight_;
    rcCalcGridSize(cfg.bmin, cfg.bmax, cfg.cs, &cfg.width, &cfg.height);
    if (cfg.width <= 0 || cfg.height <= 0 || (size_t)cfg.width * cfg.height > kMaxCells) {
        LOG_ERRORF("NavMesh", "Refusing to bake %dx%d cells; check cellSize and the scene's size", cfg.width, cfg.height);
        return nullptr;
    }

    RecastPtr<rcHeightfield, rcFreeHeightField> solid(rcAllocHeightfield());
    if (!rcCreateHeightfield(&ctx, *solid, cfg.width, cfg.height, cfg.bmin, cfg.bmax, cfg.cs, cfg.ch)) return nullptr;

    // Either facing counts; undersides have no headroom and the low-height filter drops them.
    std::vector<unsigned char> areas(triCount, RC_NULL_AREA);
    const float minUp = std::cos(kMaxSlope * 3.14159265f / 180.0f);
    for (int t = 0; t < triCount; ++t) {
        const float* a = &inputs.verts[inputs.tris[t * 3] * 3];
        const float* b = &inputs.verts[inputs.tris[t * 3 + 1] * 3];
        const float* c = &inputs.verts[inputs.tris[t * 3 + 2] * 3];
        const float e0[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
        const float e1[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
        const float n[3] = {e0[1] * e1[2] - e0[2] * e1[1], e0[2] * e1[0] - e0[0] * e1[2], e0[0] * e1[1] - e0[1] * e1[0]};
        const float length = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        if (length > 1e-9f && std::fabs(n[1]) / length >= minUp) areas[t] = RC_WALKABLE_AREA;
    }
    if (!rcRasterizeTriangles(&ctx, inputs.verts.data(), vertCount, inputs.tris.data(), areas.data(), triCount, *solid,
                              cfg.walkableClimb)) return nullptr;
    rcFilterLowHangingWalkableObstacles(&ctx, cfg.walkableClimb, *solid);
    rcFilterLedgeSpans(&ctx, cfg.walkableHeight, cfg.walkableClimb, *solid);
    rcFilterWalkableLowHeightSpans(&ctx, cfg.walkableHeight, *solid);

    RecastPtr<rcCompactHeightfield, rcFreeCompactHeightfield> chf(rcAllocCompactHeightfield());
    if (!rcBuildCompactHeightfield(&ctx, cfg.walkableHeight, cfg.walkableClimb, *solid, *chf)) return nullptr;
    solid.reset();

    // Carved before erosion so the agent radius grows them.
    for (const Area& o : inputs.obstacles) {
        const std::vector<Vector2> corners = o.polygon.Triangulate();
        for (size_t i = 0; i + 2 < corners.size(); i += 3) {
            float v[9];
            for (int k = 0; k < 3; ++k) ToNav(corners[i + k], 0.0f, &v[k * 3]);
            rcMarkConvexPolyArea(&ctx, v, 3, std::max(o.low - agentHeight_, -kFar), std::min(o.high, kFar), RC_NULL_AREA, *chf);
        }
    }
    if (!rcErodeWalkableArea(&ctx, cfg.walkableRadius, *chf)) return nullptr;

    if (!rcBuildDistanceField(&ctx, *chf)) return nullptr;
    if (!rcBuildRegions(&ctx, *chf, 0, cfg.minRegionArea, cfg.mergeRegionArea)) return nullptr;
    RecastPtr<rcContourSet, rcFreeContourSet> contours(rcAllocContourSet());
    if (!rcBuildContours(&ctx, *chf, cfg.maxSimplificationError, cfg.maxEdgeLen, *contours)) return nullptr;
    RecastPtr<rcPolyMesh, rcFreePolyMesh> polys(rcAllocPolyMesh());
    if (!rcBuildPolyMesh(&ctx, *contours, cfg.maxVertsPerPoly, *polys)) return nullptr;
    RecastPtr<rcPolyMeshDetail, rcFreePolyMeshDetail> detail(rcAllocPolyMeshDetail());
    if (!rcBuildPolyMeshDetail(&ctx, *polys, *chf, cfg.detailSampleDist, cfg.detailSampleMaxError, *detail)) return nullptr;
    if (polys->npolys == 0) return nullptr;
    for (int i = 0; i < polys->npolys; ++i) polys->flags[i] = 1;

    dtNavMeshCreateParams params;
    std::memset(&params, 0, sizeof(params));
    params.verts = polys->verts;
    params.vertCount = polys->nverts;
    params.polys = polys->polys;
    params.polyAreas = polys->areas;
    params.polyFlags = polys->flags;
    params.polyCount = polys->npolys;
    params.nvp = polys->nvp;
    params.detailMeshes = detail->meshes;
    params.detailVerts = detail->verts;
    params.detailVertsCount = detail->nverts;
    params.detailTris = detail->tris;
    params.detailTriCount = detail->ntris;
    params.walkableHeight = agentHeight_;
    params.walkableRadius = agentRadius_;
    params.walkableClimb = stepHeight_;
    rcVcopy(params.bmin, polys->bmin);
    rcVcopy(params.bmax, polys->bmax);
    params.cs = cfg.cs;
    params.ch = cfg.ch;
    params.buildBvTree = true;

    unsigned char* data = nullptr;
    int dataSize = 0;
    if (!dtCreateNavMeshData(&params, &data, &dataSize)) return nullptr;
    auto baked = std::make_unique<Mesh>();
    baked->mesh.reset(dtAllocNavMesh());
    if (dtStatusFailed(baked->mesh->init(data, dataSize, DT_TILE_FREE_DATA))) {
        dtFree(data);
        return nullptr;
    }
    baked->query.reset(dtAllocNavMeshQuery());
    if (dtStatusFailed(baked->query->init(baked->mesh.get(), 4096))) return nullptr;
    baked->filter.setIncludeFlags(1);
    return baked;
}

void NavigationSystem::Bake() {
    const auto started = std::chrono::steady_clock::now();
    ++bakeCount_;
    mesh_.reset();

    Inputs inputs = GatherInputs();
    sightBlockers_ = inputs.obstacles;
    if (inputs.tris.empty()) return;
    mesh_ = BuildMesh(inputs);
    if (!mesh_) {
        LOG_ERROR("NavMesh", "Bake failed: nothing walkable came out of the scene");
        return;
    }
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    const dtNavMesh& mesh = *mesh_->mesh;  // only the const getTile is public
    LOG_INFOF("NavMesh", "Baked %d polygons from %d triangles in %.1f ms", mesh.getTile(0)->header->polyCount,
              (int)inputs.tris.size() / 3, ms);
}

// --- Mesh helpers -----------------------------------------------------------------------

NavigationSystem::Ref NavigationSystem::NearestPoly(Vector2 p, float z, float reach, float rise, Vector3* onMesh) const {
    if (!mesh_) return 0;
    float center[3], extents[3] = {reach, rise, reach * kGround}, nearest[3];
    ToNav(p, z, center);
    dtPolyRef ref = 0;
    if (dtStatusFailed(mesh_->query->findNearestPoly(center, extents, &mesh_->filter, &ref, nearest)) || !ref) return 0;
    if (onMesh) *onMesh = FromNav(nearest);
    return ref;
}

std::optional<float> NavigationSystem::HeightOn(Ref ref, Vector2 p) const {
    float pos[3], height = 0.0f;
    ToNav(p, 0.0f, pos);
    if (dtStatusFailed(mesh_->query->getPolyHeight((dtPolyRef)ref, pos, &height))) return std::nullopt;
    return height;
}

std::vector<float> NavigationSystem::FloorsAt(Vector2 p, float low, float high) const {
    std::vector<float> heights;
    if (!mesh_) return heights;
    float center[3], extents[3] = {0.01f, (high - low) * 0.5f, 0.01f};
    ToNav(p, (low + high) * 0.5f, center);
    dtPolyRef polys[64];
    int count = 0;
    mesh_->query->queryPolygons(center, extents, &mesh_->filter, polys, &count, 64);
    for (int i = 0; i < count; ++i) {
        if (auto h = HeightOn(polys[i], p); h && *h >= low && *h <= high) heights.push_back(*h);
    }
    return heights;
}

bool NavigationSystem::StraightWalk(Ref from, Vector2 a, Vector2 b, Ref* to) const {
    if (!mesh_ || !from) return false;
    float start[3], end[3], t = 0.0f, normal[3];
    ToNav(a, 0.0f, start);
    ToNav(b, 0.0f, end);
    dtPolyRef path[kMaxPath];
    int count = 0;
    if (dtStatusFailed(mesh_->query->raycast((dtPolyRef)from, start, end, &mesh_->filter, &t, normal, path, &count, kMaxPath))) return false;
    if (t != FLT_MAX) return false;
    if (to) *to = count > 0 ? path[count - 1] : from;
    return true;
}

void NavigationSystem::ForEachSurfaceTriangle(const std::function<void(const float*, const float*, const float*)>& visit) const {
    if (!mesh_) return;
    const dtNavMesh& mesh = *mesh_->mesh;
    for (int t = 0; t < mesh.getMaxTiles(); ++t) {
        const dtMeshTile* tile = mesh.getTile(t);
        if (!tile || !tile->header) continue;
        for (int i = 0; i < tile->header->polyCount; ++i) {
            const dtPoly& poly = tile->polys[i];
            if (poly.getType() == DT_POLYTYPE_OFFMESH_CONNECTION) continue;
            const dtPolyDetail& detail = tile->detailMeshes[i];
            for (int j = 0; j < detail.triCount; ++j) {
                const unsigned char* tri = &tile->detailTris[(detail.triBase + j) * 4];
                const float* corner[3];
                for (int k = 0; k < 3; ++k) {
                    corner[k] = tri[k] < poly.vertCount ? &tile->verts[poly.verts[tri[k]] * 3]
                                                        : &tile->detailVerts[(detail.vertBase + tri[k] - poly.vertCount) * 3];
                }
                visit(corner[0], corner[1], corner[2]);
            }
        }
    }
}

void NavigationSystem::AppendHeightBends(Vector3 a, Vector3 b, std::vector<Vector3>& out) const {
    struct Sample { float along; float z; Vector2 at; };
    const Vector2 a2{a.x, a.y}, b2{b.x, b.y};
    const float length = GroundDistance(a2, b2);
    std::vector<Sample> profile{{0.0f, a.z, a2}};
    if (Ref start = NearestPoly(a2, a.z, cellSize_, stepHeight_)) {
        float s[3], e[3], t = 0.0f, normal[3];
        ToNav(a2, 0.0f, s);
        ToNav(b2, 0.0f, e);
        dtPolyRef path[kMaxPath];
        int count = 0;
        mesh_->query->raycast((dtPolyRef)start, s, e, &mesh_->filter, &t, normal, path, &count, kMaxPath);
        const int steps = (int)std::ceil(length / (cellSize_ * 0.5f));
        int k = 0;
        for (int i = 1; i < steps; ++i) {
            const float along = (float)i / steps;
            const Vector2 p = a2 + (b2 - a2) * along;
            for (int j = k; j < count; ++j) {
                if (auto h = HeightOn(path[j], p)) {
                    profile.push_back({along * length, *h, p});
                    k = j;
                    break;
                }
            }
        }
    }
    profile.push_back({length, b.z, b2});

    // Douglas-Peucker on height, so a flight of stairs becomes one ramp.
    const float tolerance = std::max(1.0f, stepHeight_ * 0.5f);
    std::vector<bool> keep(profile.size(), false);
    keep.front() = keep.back() = true;
    std::function<void(size_t, size_t)> simplify = [&](size_t i, size_t j) {
        if (j <= i + 1) return;
        const Sample& p = profile[i];
        const Sample& q = profile[j];
        float worst = tolerance;
        size_t split = 0;
        for (size_t k = i + 1; k < j; ++k) {
            const float t = q.along > p.along ? (profile[k].along - p.along) / (q.along - p.along) : 0.0f;
            const float off = std::fabs(profile[k].z - (p.z + (q.z - p.z) * t));
            if (off > worst) { worst = off; split = k; }
        }
        if (split == 0) return;
        keep[split] = true;
        simplify(i, split);
        simplify(split, j);
    };
    simplify(0, profile.size() - 1);
    for (size_t k = 1; k < profile.size(); ++k) {
        if (keep[k]) out.push_back({profile[k].at.x, profile[k].at.y, profile[k].z});
    }
}

// --- Queries ----------------------------------------------------------------------------

bool NavigationSystem::IsWalkable(Vector2 p) const {
    return !FloorsAt(p, -kFar, kFar).empty();
}

bool NavigationSystem::IsWalkable(Vector2 p, float z) const {
    return !FloorsAt(p, z - stepHeight_, z + stepHeight_).empty();
}

bool NavigationSystem::HasLineOfSight(Vector2 a, Vector2 b, float z) const {
    return StraightWalk(NearestPoly(a, z, 0.01f, stepHeight_), a, b);
}

std::optional<float> NavigationSystem::FloorHeight(Vector2 p, float z) const {
    Vector3 on;
    if (!NearestPoly(p, z, cellSize_ * 2.0f, kFar, &on)) return std::nullopt;
    return on.z;
}

Vector3 NavigationSystem::Slide(Vector3 from, Vector2 delta) const {
    Vector3 on;
    const Ref ref = NearestPoly({from.x, from.y}, from.z, cellSize_ * 2.0f, agentHeight_, &on);
    if (!ref) return from;
    float start[3], end[3], result[3];
    ToNav({on.x, on.y}, on.z, start);
    ToNav({on.x + delta.x, on.y + delta.y}, on.z, end);
    dtPolyRef visited[16];
    int count = 0;
    if (dtStatusFailed(mesh_->query->moveAlongSurface((dtPolyRef)ref, start, end, &mesh_->filter, result, visited, &count, 16))) return on;
    Vector3 out = FromNav(result);
    out.z = HeightOn(count > 0 ? visited[count - 1] : ref, {out.x, out.y}).value_or(on.z);
    return out;
}

std::vector<Vector3> NavigationSystem::FindPath(Vector2 start, Vector2 end, float startZ) const {
    if (!mesh_) return {};
    Vector3 from, goal;
    const Ref startRef = NearestPoly(start, startZ, cellSize_ * 4.0f, kFar, &from);
    Ref goalRef = 0;
    if (auto picked = PickFloor(end)) goalRef = NearestPoly({picked->x, picked->y}, picked->z, cellSize_, stepHeight_, &goal);
    if (!goalRef) goalRef = NearestPoly(end, 0.0f, cellSize_ * 6.0f, kFar, &goal);
    if (!startRef || !goalRef) return {};

    float s[3], e[3];
    ToNav({from.x, from.y}, from.z, s);
    ToNav({goal.x, goal.y}, goal.z, e);
    dtPolyRef corridor[kMaxPath];
    int corridorCount = 0;
    const dtStatus status = mesh_->query->findPath((dtPolyRef)startRef, (dtPolyRef)goalRef, s, e, &mesh_->filter, corridor, &corridorCount, kMaxPath);
    // A partial path doesn't arrive, so it isn't one.
    if (dtStatusFailed(status) || corridorCount == 0 || (status & DT_PARTIAL_RESULT)) return {};

    std::vector<float> corners(kMaxPath * 3);
    int cornerCount = 0;
    mesh_->query->findStraightPath(s, e, corridor, corridorCount, corners.data(), nullptr, nullptr, &cornerCount, kMaxPath);
    std::vector<Vector3> path;
    for (int i = 1; i < cornerCount; ++i) AppendHeightBends(FromNav(&corners[(i - 1) * 3]), FromNav(&corners[i * 3]), path);
    return path;
}

std::optional<Vector3> NavigationSystem::PickFloor(Vector2 p) const {
    constexpr float kHigh = 10000.0f;
    const Vector3 origin = World3D::ToGL(p.x, p.y + kHigh * World3D::kPitchCos, kHigh);
    const Vector3 toward = World3D::kTowardCamera;
    return PickFloor(World3D::Ray{origin, {-toward.x, -toward.y, -toward.z}});
}

std::optional<Vector3> NavigationSystem::PickFloor(const World3D::Ray& ray) const {
    float best = FLT_MAX;
    ForEachSurfaceTriangle([&](const float* a, const float* b, const float* c) {
        if (auto t = RayTriangle(ray.origin, ray.direction, a, b, c); t && *t < best) best = *t;
    });
    if (best == FLT_MAX) return std::nullopt;
    const Vector3 hit = ray.At(best);
    return Vector3{hit.x, hit.z / kGround, hit.y};
}

bool NavigationSystem::CanSee(Vector3 a, Vector3 b) const {
    const Vector2 a2{a.x, a.y}, b2{b.x, b.y};
    const int steps = std::max(1, (int)std::ceil((b2 - a2).Length() / (cellSize_ * 0.5f)));
    for (int i = 1; i < steps; ++i) {
        const float t = (float)i / steps;
        const Vector2 p = a2 + (b2 - a2) * t;
        const float z = a.z + (b.z - a.z) * t;
        if (!FloorsAt(p, z, kFar).empty()) return false;
        for (const Area& o : sightBlockers_) {
            if (z < o.low || z > o.high) continue;
            if (p.x < o.bounds.x || p.y < o.bounds.y || p.x > o.bounds.x + o.bounds.width ||
                p.y > o.bounds.y + o.bounds.height) continue;
            if (o.polygon.Contains(p)) return false;
        }
    }
    return true;
}

float NavigationSystem::GroundDistance(Vector2 a, Vector2 b) const {
    const float dx = b.x - a.x, dy = (b.y - a.y) * kGround;
    return std::sqrt(dx * dx + dy * dy);
}

// --- Reach ------------------------------------------------------------------------------

namespace {
int64_t CellKey(int cx, int cy) { return ((int64_t)cx << 32) ^ (int64_t)(uint32_t)cy; }
}  // namespace

NavigationSystem::Reach NavigationSystem::ComputeReach(Vector2 start, float z, float budget,
                                                 const std::vector<Vector3>& blockers) const {
    Reach r;
    r.bake = bakeCount_;
    r.budget = budget;
    r.blockers = blockers;
    Vector3 on;
    const Ref startRef = NearestPoly(start, z, cellSize_ * 4.0f, agentHeight_, &on);
    if (!startRef) return r;
    r.origin = {on.x, on.y};
    r.originZ = on.z;

    const float cs = cellSize_;
    auto center = [&](int cx, int cy) { return Vector2{(cx + 0.5f) * cs, (cy + 0.5f) * cs}; };
    auto blocked = [&](Vector2 p) {
        for (const Vector3& b : blockers) {
            if (GroundDistance(p, {b.x, b.y}) < b.z) return true;
        }
        return false;
    };
    auto at = [&](int index) { return index == r.start ? r.origin : center(r.nodes[index].cx, r.nodes[index].cy); };

    r.nodes.push_back({(int)std::floor(on.x / cs), (int)std::floor(on.y / cs), on.z, startRef, 0.0f, -1});
    r.cells[CellKey(r.nodes[0].cx, r.nodes[0].cy)].push_back(0);
    r.start = 0;

    struct Open { float g; int index; bool operator>(const Open& o) const { return g > o.g; } };
    std::priority_queue<Open, std::vector<Open>, std::greater<Open>> open;
    open.push({0.0f, 0});
    while (!open.empty() && (int)r.nodes.size() < kMaxReachNodes) {
        const Open node = open.top();
        open.pop();
        if (node.g > r.nodes[node.index].cost) continue;
        const Reach::Node current = r.nodes[node.index];
        const Vector2 from = at(node.index);
        for (int k = 0; k < 8; ++k) {
            const int nx = current.cx + kNeighbourDX[k], ny = current.cy + kNeighbourDY[k];
            const Vector2 to = center(nx, ny);
            if (blocked(to)) continue;
            if (k >= 4 && (blocked(center(nx, current.cy)) || blocked(center(current.cx, ny)))) continue;
            Ref poly = 0;
            if (!StraightWalk(current.poly, from, to, &poly)) continue;
            const std::optional<float> height = HeightOn(poly, to);
            if (!height) continue;
            const float g = current.cost + GroundDistance(from, to);
            if (g > budget) continue;

            std::vector<int>& cell = r.cells[CellKey(nx, ny)];
            int index = -1;
            for (int i : cell) {
                if (std::fabs(r.nodes[i].z - *height) < 1.0f) { index = i; break; }
            }
            if (index < 0) {
                index = (int)r.nodes.size();
                r.nodes.push_back({nx, ny, *height, poly, g, node.index});
                cell.push_back(index);
            } else if (g < r.nodes[index].cost) {
                r.nodes[index].cost = g;
                r.nodes[index].parent = node.index;
                r.nodes[index].poly = poly;
            } else {
                continue;
            }
            open.push({g, index});
        }
    }
    return r;
}

namespace {
int ReachNodeAt(const NavigationSystem::Reach& r, float cellSize, Vector2 p, float z, float step) {
    auto it = r.cells.find(CellKey((int)std::floor(p.x / cellSize), (int)std::floor(p.y / cellSize)));
    if (it == r.cells.end()) return -1;
    int best = -1;
    float bestRise = step;
    for (int i : it->second) {
        const float rise = std::fabs(r.nodes[i].z - z);
        if (rise <= bestRise && r.nodes[i].cost <= r.budget) { bestRise = rise; best = i; }
    }
    return best;
}
}  // namespace

std::optional<float> NavigationSystem::ReachCost(const Reach& r, Vector2 p, float z) const {
    if (r.bake != bakeCount_ || r.start < 0) return std::nullopt;
    const int node = ReachNodeAt(r, cellSize_, p, z, stepHeight_);
    if (node < 0) return std::nullopt;
    return r.nodes[node].cost;
}

std::vector<Vector3> NavigationSystem::ReachPath(const Reach& r, Vector2 p, float z) const {
    if (r.bake != bakeCount_ || r.start < 0) return {};
    const int goal = ReachNodeAt(r, cellSize_, p, z, stepHeight_);
    if (goal < 0) return {};
    const float goalZ = HeightOn(r.nodes[goal].poly, p).value_or(r.nodes[goal].z);
    if (goal == r.start) return {{p.x, p.y, goalZ}};

    std::vector<int> nodes;
    for (int i = goal; i != -1; i = r.nodes[i].parent) nodes.push_back(i);
    std::reverse(nodes.begin(), nodes.end());
    const float cs = cellSize_;
    std::vector<Vector3> raw;
    raw.reserve(nodes.size());
    for (int i : nodes) raw.push_back({(r.nodes[i].cx + 0.5f) * cs, (r.nodes[i].cy + 0.5f) * cs, r.nodes[i].z});
    raw.front() = {r.origin.x, r.origin.y, r.originZ};
    raw.back() = {p.x, p.y, goalZ};

    // String pulling, staying on each node's floor and clear of blockers.
    auto clear = [&](size_t i, size_t j) {
        const Vector2 a{raw[i].x, raw[i].y}, b{raw[j].x, raw[j].y};
        Ref arrived = 0;
        if (!StraightWalk(r.nodes[nodes[i]].poly, a, b, &arrived)) return false;
        const std::optional<float> h = HeightOn(arrived, b);
        if (!h || std::fabs(*h - raw[j].z) > 1.0f) return false;
        const int steps = std::max(1, (int)std::ceil(GroundDistance(a, b) / (cs * 0.5f)));
        for (int s = 1; s < steps; ++s) {
            const Vector2 q = a + (b - a) * ((float)s / steps);
            for (const Vector3& o : r.blockers) {
                if (GroundDistance(q, {o.x, o.y}) < o.z) return false;
            }
        }
        return true;
    };
    std::vector<Vector3> pulled;
    size_t anchor = 0;
    while (anchor < raw.size() - 1) {
        size_t farthest = anchor + 1;
        for (size_t j = raw.size() - 1; j > anchor + 1; --j) {
            if (clear(anchor, j)) { farthest = j; break; }
        }
        AppendHeightBends(raw[anchor], raw[farthest], pulled);
        anchor = farthest;
    }
    return pulled;
}

std::vector<NavigationSystem::ReachRun> NavigationSystem::ReachRuns(const Reach& r) const {
    std::vector<ReachRun> runs;
    if (r.bake != bakeCount_ || r.start < 0) return runs;
    const float cs = cellSize_;
    std::map<int, std::vector<int>> rows;
    for (int i = 0; i < (int)r.nodes.size(); ++i) {
        if (r.nodes[i].cost <= r.budget) rows[r.nodes[i].cy].push_back(i);
    }
    struct Open { ReachRun run; int lastX; };
    for (auto& [cy, row] : rows) {
        std::sort(row.begin(), row.end(), [&](int a, int b) { return r.nodes[a].cx < r.nodes[b].cx; });
        std::vector<Open> open;
        for (int i : row) {
            const Reach::Node& n = r.nodes[i];
            for (size_t k = open.size(); k-- > 0;) {
                if (open[k].lastX < n.cx - 1) {
                    runs.push_back(open[k].run);
                    open.erase(open.begin() + (long)k);
                }
            }
            bool joined = false;
            for (Open& o : open) {
                if (o.lastX == n.cx - 1 && std::fabs(o.run.z - n.z) < 0.5f) {
                    o.run.x1 = (n.cx + 1) * cs;
                    o.run.cost = std::max(o.run.cost, n.cost);
                    o.lastX = n.cx;
                    joined = true;
                    break;
                }
            }
            if (!joined) open.push_back({{n.cx * cs, (n.cx + 1) * cs, (cy + 0.5f) * cs, n.z, n.cost}, n.cx});
        }
        for (const Open& o : open) runs.push_back(o.run);
    }
    return runs;
}

// --- Debug ------------------------------------------------------------------------------

std::vector<std::vector<Vector3>> NavigationSystem::Polygons() const {
    std::vector<std::vector<Vector3>> polygons;
    if (!mesh_) return polygons;
    const dtNavMesh& mesh = *mesh_->mesh;
    for (int t = 0; t < mesh.getMaxTiles(); ++t) {
        const dtMeshTile* tile = mesh.getTile(t);
        if (!tile || !tile->header) continue;
        for (int i = 0; i < tile->header->polyCount; ++i) {
            const dtPoly& poly = tile->polys[i];
            if (poly.getType() == DT_POLYTYPE_OFFMESH_CONNECTION) continue;
            std::vector<Vector3> out;
            for (int j = 0; j < poly.vertCount; ++j) out.push_back(FromNav(&tile->verts[poly.verts[j] * 3]));
            polygons.push_back(std::move(out));
        }
    }
    return polygons;
}

// --- Movers -----------------------------------------------------------------------------

void NavigationSystem::SlideMovers() {
    if (!mesh_) return;
    world->Query<TransformComponent, KinematicsComponent, MovementComponent>([&](Entity e, auto& t, auto& kin, auto&) {
        const Vector2 pos{t.worldX, t.worldY};
        if (IsWalkable(pos, t.worldZ)) { lastWalkablePos_[e] = pos; return; }

        // Off the mesh: replay this frame's step from the last walkable spot as a slide.
        const auto it = lastWalkablePos_.find(e);
        const Vector2 last = it != lastWalkablePos_.end() ? it->second : pos;
        const Vector3 slid = Slide({last.x, last.y, t.worldZ}, pos - last);
        if (std::fabs(slid.x - pos.x) > 0.01f) kin.velocity.x = 0.0f;
        if (std::fabs(slid.y - pos.y) > 0.01f) kin.velocity.y = 0.0f;
        t.localX += slid.x - pos.x;
        t.localY += slid.y - pos.y;
        t.worldX = slid.x;
        t.worldY = slid.y;
        lastWalkablePos_[e] = {slid.x, slid.y};
    });
}

void NavigationSystem::DrawPaths() {
    auto* render = scene->GetSystem<RenderSystem>();
    if (!render) return;
    const Color color{255, 255, 255, 200};
    world->Query<TransformComponent, MovementComponent>([&](Entity, auto& t, auto& mv) {
        Vector2 prev = World3D::To2D(World3D::ToGL(t.worldX, t.worldY, t.worldZ));
        for (int i = std::max(0, mv.currentWaypointIndex); i < (int)mv.waypoints.size(); ++i) {
            const Vector3& p = mv.waypoints[i];
            const Vector2 wp = World3D::To2D(World3D::ToGL(p.x, p.y, p.z));
            render->IssueDrawCommand(DrawLineCmd{"selection", prev.x, prev.y, wp.x, wp.y, color});
            render->IssueDrawCommand(DrawCircleCmd{"selection", wp.x, wp.y, 3.0f, color});
            prev = wp;
        }
    });
}

}  // namespace Elysium::Systems

REGISTER_SYSTEM(Elysium::Systems::NavigationSystem)
