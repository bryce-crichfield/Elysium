
float SceneDistance(vec2 p)
{
    vec2 q = abs(p) - e_Size * 0.5;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0);
}
