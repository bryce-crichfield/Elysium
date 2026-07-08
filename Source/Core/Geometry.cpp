#include "Core/Geometry.h"
#include "Services/LogService.h"
#include <algorithm>

namespace Elysium {

namespace {

// 2D cross product of (a-o) and (b-o). Positive means a left ("counter-clockwise") turn at o->a->b.
float Cross(const Vector2& o, const Vector2& a, const Vector2& b) {
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

bool PointInTriangle(const Vector2& p, const Vector2& a, const Vector2& b, const Vector2& c) {
    float d1 = Cross(a, b, p);
    float d2 = Cross(b, c, p);
    float d3 = Cross(c, a, p);
    bool hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(hasNeg && hasPos);
}

}  // namespace

bool IsClockwise(const std::vector<Vector2>& points) {
    double sum = 0.0;
    size_t n = points.size();
    for (size_t i = 0; i < n; i++) {
        const Vector2& a = points[i];
        const Vector2& b = points[(i + 1) % n];
        sum += (double)(b.x - a.x) * (double)(b.y + a.y);
    }
    return sum > 0.0;
}

std::vector<Vector2> TriangulatePolygon(const std::vector<Vector2>& points) {
    std::vector<Vector2> triangles;
    size_t n = points.size();
    if (n < 3) return triangles;

    std::vector<Vector2> poly = points;
    if (IsClockwise(poly)) {
        std::reverse(poly.begin(), poly.end());
    }

    std::vector<int> remaining(poly.size());
    for (size_t i = 0; i < poly.size(); i++) remaining[i] = (int)i;

    while (remaining.size() > 3) {
        size_t m = remaining.size();
        bool clipped = false;

        for (size_t i = 0; i < m; i++) {
            size_t iPrev = (i + m - 1) % m;
            size_t iNext = (i + 1) % m;
            const Vector2& prev = poly[remaining[iPrev]];
            const Vector2& curr = poly[remaining[i]];
            const Vector2& next = poly[remaining[iNext]];

            // Convexity: with normalized (CCW) winding, a convex vertex turns left at curr.
            if (Cross(prev, curr, next) <= 0.0f) continue;

            bool isEar = true;
            for (size_t j = 0; j < m; j++) {
                if (j == iPrev || j == i || j == iNext) continue;
                if (PointInTriangle(poly[remaining[j]], prev, curr, next)) {
                    isEar = false;
                    break;
                }
            }

            if (isEar) {
                // Emitted in reverse (prev, next, curr) — raylib's default rlgl state culls
                // counter-clockwise-per-this-Cross-formula triangles as back-facing in its
                // Y-down screen space, so the renderable winding is the opposite of the one
                // this function's internal ear-finding logic normalizes to.
                triangles.push_back(prev);
                triangles.push_back(next);
                triangles.push_back(curr);
                remaining.erase(remaining.begin() + i);
                clipped = true;
                break;
            }
        }

        if (!clipped) {
            LOG_WARNINGF("Geometry", "TriangulatePolygon: no ear found among %zu remaining vertices; polygon may be self-intersecting or degenerate",
                         remaining.size());
            break;
        }
    }

    if (remaining.size() == 3) {
        triangles.push_back(poly[remaining[0]]);
        triangles.push_back(poly[remaining[2]]);
        triangles.push_back(poly[remaining[1]]);
    }

    return triangles;
}

bool PointInPolygon(Vector2 point, const std::vector<Vector2>& points) {
    bool inside = false;
    size_t n = points.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const Vector2& a = points[i];
        const Vector2& b = points[j];
        bool crosses = ((a.y > point.y) != (b.y > point.y));
        if (crosses) {
            float xIntersect = a.x + (point.y - a.y) * (b.x - a.x) / (b.y - a.y);
            if (point.x < xIntersect) inside = !inside;
        }
    }
    return inside;
}

}  // namespace Elysium
