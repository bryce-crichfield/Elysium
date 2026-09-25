
// Exact signed distance to a simple polygon (Inigo Quilez's sdPolygon): the distance is
// the nearest edge, the sign comes from a crossing test in the same loop (even-odd, so
// a self-intersecting outline alternates inside/outside). Vertices are relative to the
// box center, in order, at most 64 (kMaxSdfPolygonPoints). O(N) per fragment.
uniform vec2 e_Points[64];
uniform int  e_PointCount;

float SceneDistance(vec2 p)
{
    int n = clamp(e_PointCount, 0, 64);
    if (n < 3) return 1e5;

    float d = dot(p - e_Points[0], p - e_Points[0]);
    float s = 1.0;
    for (int i = 0, j = n - 1; i < n; j = i, i++)
    {
        vec2 e = e_Points[j] - e_Points[i];
        vec2 w = p - e_Points[i];
        vec2 b = w - e * clamp(dot(w, e) / max(dot(e, e), 1e-8), 0.0, 1.0);
        d = min(d, dot(b, b));

        bvec3 c = bvec3(p.y >= e_Points[i].y, p.y < e_Points[j].y, e.x * w.y > e.y * w.x);
        if (all(c) || all(not(c))) s = -s;
    }
    return s * sqrt(d);
}
