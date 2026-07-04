#pragma once

#include <vector>
#include "raylib.h"

namespace Elysium {

// Returns true if the polygon is wound clockwise in raylib's screen-space (Y-down) coordinates.
bool IsClockwise(const std::vector<Vector2>& points);

// Ear-clipping triangulation for a simple (non-self-intersecting) polygon, convex or concave.
// Accepts either winding order. Returns a flat list of triangle vertices (3 per triangle),
// wound so raylib's default backface culling (front face = CCW in GL clip space) renders them
// in raylib's Y-down screen space.
// Returns an empty vector if fewer than 3 points are given.
std::vector<Vector2> TriangulatePolygon(const std::vector<Vector2>& points);

}  // namespace Elysium
