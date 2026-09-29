#include "Editor/PolygonHandles.h"

namespace Elysium {

std::optional<size_t> PolygonHandles::HitVertex(const std::vector<Vector2>& polygon, Vector2 mouseWorld, float radiusWorld) {
    for (size_t i = 0; i < polygon.size(); ++i) {
        if ((polygon[i] - mouseWorld).Length() <= radiusWorld) return i;
    }
    return std::nullopt;
}

bool PolygonHandles::Begin(const std::vector<Vector2>& polygon, Vector2 mouseWorld, float radiusWorld) {
    vertex_ = HitVertex(polygon, mouseWorld, radiusWorld);
    return vertex_.has_value();
}

void PolygonHandles::Drag(std::vector<Vector2>& polygon, Vector2 mouseWorld) const {
    if (vertex_ && *vertex_ < polygon.size()) polygon[*vertex_] = mouseWorld;
}

}  // namespace Elysium
