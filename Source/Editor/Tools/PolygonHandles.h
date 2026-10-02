#pragma once
#include <optional>
#include <vector>
#include "Core/Math/MathTypes.h"

namespace Elysium {

// Vertex-drag editing of a world-space polygon: call Begin on a click that lands on a
// handle, then Drag every frame while the mouse is held.
class PolygonHandles {
public:
    static std::optional<size_t> HitVertex(const std::vector<Vector2>& polygon, Vector2 mouseWorld, float radiusWorld);

    bool Begin(const std::vector<Vector2>& polygon, Vector2 mouseWorld, float radiusWorld);
    bool Dragging() const { return vertex_.has_value(); }
    void Drag(std::vector<Vector2>& polygon, Vector2 mouseWorld) const;
    void End() { vertex_.reset(); }

private:
    std::optional<size_t> vertex_;
};

}  // namespace Elysium
