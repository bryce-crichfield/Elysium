#include "Core/Geometry.h"
#include "Core/Log.h"
#include <algorithm>
#include <cmath>
#include <sstream>

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

Rectangle PolygonBounds(const std::vector<Vector2>& points) {
    if (points.empty()) return Rectangle{0, 0, 0, 0};
    float minX = points[0].x, maxX = points[0].x, minY = points[0].y, maxY = points[0].y;
    for (const auto& p : points) {
        minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
    }
    return Rectangle{minX, minY, maxX - minX, maxY - minY};
}

std::vector<Vector2> ConvexHull(std::vector<Vector2> pts) {
    if (pts.size() < 3) return pts;
    std::sort(pts.begin(), pts.end(), [](const Vector2& a, const Vector2& b) {
        return a.x < b.x || (a.x == b.x && a.y < b.y);
    });
    pts.erase(std::unique(pts.begin(), pts.end()), pts.end());
    if (pts.size() < 3) return pts;

    std::vector<Vector2> hull(pts.size() * 2);
    size_t k = 0;
    for (size_t i = 0; i < pts.size(); ++i) {
        while (k >= 2 && Cross(hull[k - 2], hull[k - 1], pts[i]) <= 0) k--;
        hull[k++] = pts[i];
    }
    for (size_t i = pts.size() - 1, t = k + 1; i > 0; --i) {
        while (k >= t && Cross(hull[k - 2], hull[k - 1], pts[i - 1]) <= 0) k--;
        hull[k++] = pts[i - 1];
    }
    hull.resize(k - 1);
    return hull;
}

bool ClosestPointOnPolygon(Vector2 point, const std::vector<Vector2>& points, ClosestPoint& out, Vector2 scale) {
    if (points.empty()) return false;
    // Insideness is unaffected by an axis scale, so test it before stretching.
    out.inside = points.size() >= 3 && PointInPolygon(point, points);

    // A zero axis would collapse the space and divide by zero on the way back out.
    scale.x = scale.x != 0.0f ? scale.x : 1.0f;
    scale.y = scale.y != 0.0f ? scale.y : 1.0f;
    auto stretch = [&](Vector2 p) { return Vector2{p.x * scale.x, p.y * scale.y}; };
    auto unstretch = [&](Vector2 p) { return Vector2{p.x / scale.x, p.y / scale.y}; };

    const Vector2 scaled = stretch(point);
    float best = 1e30f;
    Vector2 bestPoint{};
    size_t n = points.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const Vector2 a = stretch(points[j]);
        const Vector2 b = stretch(points[i]);
        Vector2 ab = b - a;
        float len2 = ab.x * ab.x + ab.y * ab.y;
        float t = len2 > 0.0f ? std::clamp(Dot(scaled - a, ab) / len2, 0.0f, 1.0f) : 0.0f;
        Vector2 closest = a + ab * t;
        float d = (scaled - closest).Length();
        if (d < best) { best = d; bestPoint = closest; }
    }
    out.distance = best;
    out.closest = unstretch(bestPoint);
    return true;
}

float DistanceToPolygon(Vector2 point, const std::vector<Vector2>& points, Vector2 scale) {
    if (points.empty()) return 1e30f;
    ClosestPoint result;
    if (!ClosestPointOnPolygon(point, points, result, scale)) return 1e30f;
    return result.inside ? 0.0f : result.distance;
}

std::vector<Vector2> TranslatePolygon(const std::vector<Vector2>& points, Vector2 delta) {
    std::vector<Vector2> out;
    out.reserve(points.size());
    for (const auto& p : points) out.push_back(p + delta);
    return out;
}

std::vector<Vector2> ParsePointList(const std::string& text) {
    std::vector<Vector2> points;
    std::istringstream tokens(text);
    std::string token;
    while (tokens >> token) {
        size_t comma = token.find(',');
        if (comma == std::string::npos) continue;
        try {
            points.push_back({std::stof(token.substr(0, comma)), std::stof(token.substr(comma + 1))});
        } catch (...) {
            continue;
        }
    }
    return points;
}

std::string FormatPointList(const std::vector<Vector2>& points) {
    std::ostringstream ss;
    for (size_t i = 0; i < points.size(); i++) {
        if (i > 0) ss << ' ';
        ss << points[i].x << ',' << points[i].y;
    }
    return ss.str();
}

std::vector<Vector2> IsoDiamond(float w, float h) {
    return { {0.0f, -h * 0.5f}, {w * 0.5f, 0.0f}, {0.0f, h * 0.5f}, {-w * 0.5f, 0.0f} };
}

}  // namespace Elysium
