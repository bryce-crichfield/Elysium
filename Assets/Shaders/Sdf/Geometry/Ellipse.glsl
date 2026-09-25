
// Approximate (not exact) ellipse distance — good near the boundary, which is all
// coverage/stroke need; glow falloff far from the edge is slightly off.
float SceneDistance(vec2 p)
{
    vec2 r = max(e_Size * 0.5, vec2(1e-4));
    float k0 = length(p / r);
    float k1 = length(p / (r * r));
    return k0 * (k0 - 1.0) / max(k1, 1e-4);
}
