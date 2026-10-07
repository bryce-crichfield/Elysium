
float SceneDistance(vec2 p)
{
    return length(p) - min(e_Size.x, e_Size.y) * 0.5;
}
