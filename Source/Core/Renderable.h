#pragma once

#include <cstdint>
#include <optional>
#include <vector>
#include "Core/Graphics.h"
#include "Core/MathTypes.h"
#include "Core/Entity.h"

namespace Elysium {

class World;
class RenderContext;
class ServiceLocator;

using RenderableTypeId = uint32_t;

// One entry in a frame's sorted draw queue. ECS-backed: entity valid, payload null.
// Value-backed (script draw command): entity invalid, payload points at the command struct.
struct RenderRecord {
    uint8_t layerIndex = 0;
    uint8_t hierarchyDepth = 0;
    uint8_t childIndex = 0;
    bool isWorldSpace = false;
    float x = 0.0f;
    float y = 0.0f;
    uint32_t collectionOrder = 0;    // monotonic; final sort tiebreak
    RenderableTypeId typeId = 0;
    Entity entity = INVALID_ENTITY;
    const void* payload = nullptr;
};

// A shape's analytic description for the SDF material path. `geometry` names a chunk in
// Assets/Shaders/Sdf/Geometry; the rest feeds that chunk's e_ uniforms.
struct SdfGeometry {
    const char* geometry = nullptr;   // "Rect" | "RoundedRect" | "Circle" | "Ellipse" | "Segment" | "Polygon"
    Rectangle box{};                  // draw-space box the shape is centered in
    float cornerRadius = 0.0f;        // world units (RoundedRect)
    Vector2 pointA{}, pointB{};       // relative to box center (Segment)
    float thickness = 0.0f;           // (Segment)
    std::vector<Vector2> points;      // relative to box center (Polygon), <= kMaxSdfPolygonPoints
};

inline constexpr int kMaxSdfPolygonPoints = 64;  // matches e_Points[] in Geometry/Polygon.glsl

// A registered "kind" of thing that can appear in the render queue.
struct RenderableType {
    bool (*Has)(const World&, Entity) = nullptr;   // null for value-backed types
    void (*Render)(RenderContext&, const RenderRecord&) = nullptr;
    bool (*Pick)(const World&, const RenderRecord&, Vector2 testPos) = nullptr;
    std::optional<Rectangle> (*Bounds)(const World&, const RenderRecord&) = nullptr;
    bool skipFrustumCull = false;
    // Null for renderables with no closed-form distance (text, tiles, lights); those
    // ignore MaterialComponent and keep drawing through Render.
    bool (*Geometry)(const World&, const RenderRecord&, SdfGeometry&) = nullptr;
};

// The at-runtime collection of all "kinds" of things that can be rendered.  See REGISTER_RENDERABLE
class RenderableRegistry {
public:
    static RenderableRegistry& Instance() {
        static RenderableRegistry instance;
        return instance;
    }

    RenderableTypeId Register(RenderableType type) {
        types_.push_back(type);
        return (RenderableTypeId)(types_.size() - 1);
    }

    const RenderableType& Get(RenderableTypeId id) const { return types_[id]; }
    const std::vector<RenderableType>& All() const { return types_; }

private:
    std::vector<RenderableType> types_;
};

}  // namespace Elysium

#define RENDERABLE_REGISTER_CONCAT_IMPL(x, y) x##y
#define RENDERABLE_REGISTER_CONCAT(x, y) RENDERABLE_REGISTER_CONCAT_IMPL(x, y)

// Registers an ECS-component-backed renderable type. Pick/Bounds/Geometry may be nullptr.
#define REGISTER_RENDERABLE(HasFn, RenderFn, PickFn, BoundsFn, SkipCull, GeometryFn)               \
    static bool RENDERABLE_REGISTER_CONCAT(_registered_renderable_, __COUNTER__) = [] {            \
        ::Elysium::RenderableRegistry::Instance().Register(                                        \
            ::Elysium::RenderableType{ HasFn, RenderFn, PickFn, BoundsFn, SkipCull, GeometryFn }); \
        return true;                                                                               \
    }();
