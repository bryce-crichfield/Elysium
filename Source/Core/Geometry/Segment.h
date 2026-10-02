#pragma once

#include <algorithm>
#include "Core/MathTypes.h"

namespace Elysium {

// A line segment from `a` to `b`.
struct Segment {
    Vector2 a{};
    Vector2 b{};

    Vector2 Direction() const { return b - a; }
    float Length() const { return Direction().Length(); }

    // Where along the segment `point` projects, clamped to it: 0 at `a`, 1 at `b`. A
    // zero-length segment projects everything onto `a`.
    float Project(Vector2 point) const {
        const Vector2 d = Direction();
        const float lengthSquared = Dot(d, d);
        return lengthSquared > 0.0f ? std::clamp(Dot(point - a, d) / lengthSquared, 0.0f, 1.0f) : 0.0f;
    }
    Vector2 At(float t) const { return a + Direction() * t; }
    Vector2 ClosestPoint(Vector2 point) const { return At(Project(point)); }
    float Distance(Vector2 point) const { return (point - ClosestPoint(point)).Length(); }

    // The segment with both ends multiplied per axis by `scale`.
    Segment Scaled(Vector2 scale) const { return {{a.x * scale.x, a.y * scale.y}, {b.x * scale.x, b.y * scale.y}}; }
};

}  // namespace Elysium
