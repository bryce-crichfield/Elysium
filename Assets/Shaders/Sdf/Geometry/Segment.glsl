
// Capsule around the segment e_PointA..e_PointB, e_Thickness wide.
float SceneDistance(vec2 p)
{
    vec2 pa = p - e_PointA;
    vec2 ba = e_PointB - e_PointA;
    float h = clamp(dot(pa, ba) / max(dot(ba, ba), 1e-4), 0.0, 1.0);
    return length(pa - ba * h) - e_Thickness * 0.5;
}
