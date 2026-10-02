#include "Core/Math/Polygon.h"

#include <algorithm>
#include <sstream>
#include "Core/Log.h"

namespace Elysium {

namespace {

// 2D cross product of (a - o) and (b - o). Positive is a left ("counter-clockwise") turn o -> a -> b.
float Cross(const Vector2& o, const Vector2& a, const Vector2& b) {
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

bool InTriangle(const Vector2& p, const Vector2& a, const Vector2& b, const Vector2& c) {
    const float d1 = Cross(a, b, p), d2 = Cross(b, c, p), d3 = Cross(c, a, p);
    const bool hasNeg = d1 < 0 || d2 < 0 || d3 < 0;
    const bool hasPos = d1 > 0 || d2 > 0 || d3 > 0;
    return !(hasNeg && hasPos);
}

}  // namespace

Polygon Polygon::Parse(const std::string& text) {
    std::vector<Vector2> points;
    std::istringstream tokens(text);
    std::string token;
    while (tokens >> token) {
        const size_t comma = token.find(',');
        if (comma == std::string::npos) continue;
        try {
            points.push_back({std::stof(token.substr(0, comma)), std::stof(token.substr(comma + 1))});
        } catch (...) {
            continue;
        }
    }
    return Polygon(std::move(points));
}

std::string Polygon::Format() const {
    std::ostringstream ss;
    for (size_t i = 0; i < points_.size(); i++) {
        if (i > 0) ss << ' ';
        ss << points_[i].x << ',' << points_[i].y;
    }
    return ss.str();
}

Polygon Polygon::IsoDiamond(float width, float height) {
    return {{0.0f, -height * 0.5f}, {width * 0.5f, 0.0f}, {0.0f, height * 0.5f}, {-width * 0.5f, 0.0f}};
}

Polygon Polygon::FromRectangle(const Rectangle& r) {
    return {{r.x, r.y}, {r.x + r.width, r.y}, {r.x + r.width, r.y + r.height}, {r.x, r.y + r.height}};
}

Polygon Polygon::ConvexHull(std::vector<Vector2> pts) {
    std::sort(pts.begin(), pts.end(), [](const Vector2& a, const Vector2& b) {
        return a.x < b.x || (a.x == b.x && a.y < b.y);
    });
    pts.erase(std::unique(pts.begin(), pts.end()), pts.end());
    if (pts.size() < 3) return Polygon(std::move(pts));

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
    return Polygon(std::move(hull));
}

bool Polygon::IsClockwise() const {
    double sum = 0.0;
    for (size_t i = 0; i < EdgeCount(); i++) {
        const Segment e = Edge(i);
        sum += (double)(e.b.x - e.a.x) * (double)(e.b.y + e.a.y);
    }
    return sum > 0.0;
}

Vector2 Polygon::Centroid() const {
    if (points_.empty()) return {0.0f, 0.0f};
    Vector2 sum{0.0f, 0.0f};
    for (const auto& p : points_) sum += p;
    return sum / (float)points_.size();
}

Rectangle Polygon::Bounds() const {
    if (points_.empty()) return Rectangle{0, 0, 0, 0};
    float minX = points_[0].x, maxX = points_[0].x, minY = points_[0].y, maxY = points_[0].y;
    for (const auto& p : points_) {
        minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
    }
    return Rectangle{minX, minY, maxX - minX, maxY - minY};
}

Polygon Polygon::Translated(Vector2 delta) const {
    std::vector<Vector2> out;
    out.reserve(points_.size());
    for (const auto& p : points_) out.push_back(p + delta);
    return Polygon(std::move(out));
}

bool Polygon::Contains(Vector2 point) const {
    bool inside = false;
    for (size_t i = 0; i < EdgeCount(); i++) {
        const Segment e = Edge(i);
        if ((e.a.y > point.y) != (e.b.y > point.y)) {
            const float xIntersect = e.a.x + (point.y - e.a.y) * (e.b.x - e.a.x) / (e.b.y - e.a.y);
            if (point.x < xIntersect) inside = !inside;
        }
    }
    return inside;
}

std::optional<Polygon::Nearest> Polygon::NearestOnOutline(Vector2 point, Vector2 scale) const {
    if (points_.empty()) return std::nullopt;
    Nearest best;
    // Insideness is unaffected by an axis scale, so test it before stretching.
    best.inside = IsValid() && Contains(point);

    // A zero axis would collapse the space and divide by zero on the way back out.
    scale.x = scale.x != 0.0f ? scale.x : 1.0f;
    scale.y = scale.y != 0.0f ? scale.y : 1.0f;
    const Vector2 scaled{point.x * scale.x, point.y * scale.y};

    best.distance = 1e30f;
    for (size_t i = 0; i < EdgeCount(); i++) {
        const Segment edge = Edge(i).Scaled(scale);
        const Vector2 closest = edge.ClosestPoint(scaled);
        const float d = (scaled - closest).Length();
        if (d < best.distance) {
            best.distance = d;
            best.point = {closest.x / scale.x, closest.y / scale.y};
            best.edge = i;
        }
    }
    return best;
}

float Polygon::Distance(Vector2 point, Vector2 scale) const {
    const auto nearest = NearestOnOutline(point, scale);
    if (!nearest) return 1e30f;
    return nearest->inside ? 0.0f : nearest->distance;
}

std::vector<Vector2> Polygon::Triangulate() const {
    std::vector<Vector2> triangles;
    if (!IsValid()) return triangles;

    std::vector<Vector2> poly = points_;
    if (IsClockwise()) std::reverse(poly.begin(), poly.end());

    std::vector<int> remaining(poly.size());
    for (size_t i = 0; i < poly.size(); i++) remaining[i] = (int)i;

    while (remaining.size() > 3) {
        const size_t m = remaining.size();
        bool clipped = false;

        for (size_t i = 0; i < m; i++) {
            const size_t iPrev = (i + m - 1) % m;
            const size_t iNext = (i + 1) % m;
            const Vector2& prev = poly[remaining[iPrev]];
            const Vector2& curr = poly[remaining[i]];
            const Vector2& next = poly[remaining[iNext]];

            // Convexity: with normalized (CCW) winding, a convex vertex turns left at curr.
            if (Cross(prev, curr, next) <= 0.0f) continue;

            bool isEar = true;
            for (size_t j = 0; j < m; j++) {
                if (j == iPrev || j == i || j == iNext) continue;
                if (InTriangle(poly[remaining[j]], prev, curr, next)) {
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
            LOG_WARNINGF("Geometry", "Polygon::Triangulate: no ear found among %zu remaining vertices; polygon may be self-intersecting or degenerate",
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

}  // namespace Elysium
