#pragma once

#include <vector>
#include <string>
#include "Core/MathTypes.h"

namespace Elysium {
// Returns true if the polygon is wound clockwise in raylib's screen-space (Y-down) coordinates.
bool IsClockwise(const std::vector<Vector2>& points);

// Ear-clipping triangulation for a simple (non-self-intersecting) polygon, convex or concave.
// Accepts either winding order. Returns a flat list of triangle vertices (3 per triangle),
// wound so raylib's default backface culling (front face = CCW in GL clip space) renders them
// in raylib's Y-down screen space.
// Returns an empty vector if fewer than 3 points are given.
std::vector<Vector2> TriangulatePolygon(const std::vector<Vector2>& points);

// Even-odd ray casting test. `points` describes a simple polygon in the same
// space as `point`. Works for either winding order.
bool PointInPolygon(Vector2 point, const std::vector<Vector2>& points);

// Axis-aligned bounds of a point set. Returns a zero rect for an empty set.
Rectangle PolygonBounds(const std::vector<Vector2>& points);

// Andrew's monotone-chain convex hull. Output is CCW in Y-down screen space, no duplicates.
std::vector<Vector2> ConvexHull(std::vector<Vector2> points);

// Shortest distance from a point to a closed polygon's outline (0 if inside).
// `scale` stretches the space the distance is measured in, so a caller working in an
// isometric projection can pass {1, tileW/tileH} and get a distance whose reach along y is
// divided by that ratio — a circle in ground space rather than a circle in screen pixels.
float DistanceToPolygon(Vector2 point, const std::vector<Vector2>& points, Vector2 scale = {1.0f, 1.0f});

// Where on the polygon's outline `point` is closest, and how far. Unlike DistanceToPolygon
// this keeps going when the point is inside, so a caller resolving penetration gets the
// nearest way out; `inside` says which side it started on. `distance` is measured in scaled
// space, `closest` is returned unscaled. False for a polygon with fewer than 2 points.
struct ClosestPoint {
    Vector2 closest{};
    float distance = 0.0f;
    bool inside = false;
};
bool ClosestPointOnPolygon(Vector2 point, const std::vector<Vector2>& points, ClosestPoint& out,
                           Vector2 scale = {1.0f, 1.0f});

// Offsets every point by `delta`.
std::vector<Vector2> TranslatePolygon(const std::vector<Vector2>& points, Vector2 delta);

// Shared XML/Lua point-list format: "x,y x,y x,y". Used by Collider/NavArea outlines.
std::vector<Vector2> ParsePointList(const std::string& text);
std::string FormatPointList(const std::vector<Vector2>& points);

// An isometric diamond centred on the origin, `w` wide and `h` tall.
std::vector<Vector2> IsoDiamond(float w, float h);

}  // namespace Elysium
