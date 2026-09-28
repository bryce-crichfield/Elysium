// Defaults for the lit-layer hooks a material didn't define.
#ifndef HAS_SURFACE_NORMAL
vec3 SurfaceNormal(float sd, vec2 p, vec2 uv, vec4 color) { return vec3(0.0, 0.0, 1.0); }
#endif
#ifndef HAS_SURFACE_EMISSION
vec3 SurfaceEmission(float sd, vec2 p, vec2 uv, vec4 color) { return vec3(0.0); }
#endif


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

    // One of three surfaces, per the E_OUTPUT_ define ShaderAsset prepends: lit layers draw
    // everything three times (RenderCompositor::RenderLit). Alpha is the coverage in all
    // three, so a sprite drawn over another hides its normals and glow too.
#if defined(E_OUTPUT_Normal)
  #if defined(SURFACE_NO_NORMAL)
    finalColor = vec4(0.0);
  #else
    vec3 n = SurfaceNormal(sd, p, uv, color);
    // Quad frame -> screen: mirror by the scale's signs, then rotate. The draw rotation
    // turns clockwise on a y-down screen, which is the reverse in these y-up normals.
    n.xy *= e_NormalXform.zw;
    vec2 c = e_NormalXform.xy;
    n.xy = vec2(n.x * c.x + n.y * c.y, -n.x * c.y + n.y * c.x);
    finalColor = vec4(normalize(n) * 0.5 + 0.5, color.a * colDiffuse.a * fragColor.a);
  #endif
#elif defined(E_OUTPUT_Emission)
    finalColor = vec4(SurfaceEmission(sd, p, uv, color), color.a * colDiffuse.a * fragColor.a);
#else
    finalColor = color * colDiffuse * fragColor;
#endif
}
