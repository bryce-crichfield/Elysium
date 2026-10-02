#pragma once

#include <initializer_list>
#include <optional>
#include <string>
#include <vector>
#include "Core/Geometry/Segment.h"
#include "Core/MathTypes.h"

namespace Elysium {

// A closed polygon: an ordered ring of points, the last joining back to the first. Simple
// (non-self-intersecting) polygons of either winding are supported throughout. "Clockwise" is
// as seen in the engine's Y-down picture.
class Polygon {
public:
    Polygon() = default;
    explicit Polygon(std::vector<Vector2> points) : points_(std::move(points)) {}
    Polygon(std::initializer_list<Vector2> points) : points_(points) {}

    // The XML/Lua point-list format "x,y x,y x,y"; malformed tokens are skipped.
    static Polygon Parse(const std::string& text);
    std::string Format() const;

    // An isometric diamond centred on the origin, `width` wide and `height` tall.
    static Polygon IsoDiamond(float width, float height);
    // The rectangle's four corners, clockwise from its top left.
    static Polygon FromRectangle(const Rectangle& rect);
    // Andrew's monotone chain: the convex hull of `points`, counter-clockwise, no duplicates.
    static Polygon ConvexHull(std::vector<Vector2> points);

    const std::vector<Vector2>& Points() const { return points_; }
    std::vector<Vector2>& Points() { return points_; }
    size_t Size() const { return points_.size(); }
    bool Empty() const { return points_.empty(); }
    // Three points make an area; anything less is a degenerate outline.
    bool IsValid() const { return points_.size() >= 3; }
    const Vector2& operator[](size_t i) const { return points_[i]; }
    Vector2& operator[](size_t i) { return points_[i]; }
    auto begin() const { return points_.begin(); }
    auto end() const { return points_.end(); }

    // Edge i runs from point i to point i + 1 (the last back to the first).
    size_t EdgeCount() const { return points_.size(); }
    Segment Edge(size_t i) const { return {points_[i], points_[(i + 1) % points_.size()]}; }

    bool IsClockwise() const;
    Vector2 Centroid() const;  // the mean of the points; the origin when empty
    Rectangle Bounds() const;  // a zero rect when empty
    Polygon Translated(Vector2 delta) const;

    // Even-odd ray cast.
    bool Contains(Vector2 point) const;

    // The nearest point on the outline to `point`. Inside or out, so a caller resolving
    // penetration still gets the nearest way out. `scale` stretches the space distances are
    // measured in: an isometric caller passes {1, tileW/tileH} for a circle on the ground rather
    // than one in screen pixels. `distance` is in that space, `point` is returned unscaled.
    struct Nearest {
        Vector2 point{};
        float distance = 0.0f;
        size_t edge = 0;  // the edge it lies on
        bool inside = false;
    };
    std::optional<Nearest> NearestOnOutline(Vector2 point, Vector2 scale = {1.0f, 1.0f}) const;
    // How far `point` is from the polygon in the same scaled space: 0 inside, huge when empty.
    float Distance(Vector2 point, Vector2 scale = {1.0f, 1.0f}) const;

    // Ear clipping: a flat triangle list (3 points each), wound so the default backface culling
    // draws them in the Y-down picture. Empty for an invalid polygon.
    std::vector<Vector2> Triangulate() const;

private:
    std::vector<Vector2> points_;
};

}  // namespace Elysium
