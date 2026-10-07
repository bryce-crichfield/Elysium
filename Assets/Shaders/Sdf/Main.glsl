void main()
{
    vec2 p = (fragTexCoord - 0.5) * e_QuadSize;
    vec2 uv = p / max(e_Size, vec2(1e-4)) + 0.5;
    float sd = SceneDistance(p);
    vec4 color = Shade(sd, p, uv);

    // Quad-edge fade. Every layer is drawn on the shape's box grown by the component's
    // padding, so any effect still visible at the quad's border (glow, fire, rings) would
    // be cut off in a hard line. Fade alpha to zero across the outer part of the padding
    // instead. The fade band never exceeds the padding, so it can't reach the shape
    // itself — and with no padding (plain fills, backdrops) it's a no-op.
    float padding = min(e_QuadSize.x - e_Size.x, e_QuadSize.y - e_Size.y) * 0.5;
    if (padding > 0.5)
    {
        vec2 toQuadEdge = e_QuadSize * 0.5 - abs(p);
        float band = min(padding * 0.6, 48.0);
        color.a *= smoothstep(0.0, band, min(toQuadEdge.x, toQuadEdge.y));
    }

    finalColor = color * colDiffuse * fragColor;
}
