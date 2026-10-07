
float SceneDistance(vec2 p)
{
    float r = min(e_CornerRadius, min(e_Size.x, e_Size.y) * 0.5);
    vec2 q = abs(p) - e_Size * 0.5 + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}
