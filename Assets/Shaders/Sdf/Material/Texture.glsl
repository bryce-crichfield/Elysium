// An image, or a sub-image of one (a sprite-sheet frame), stretched over the shape's
// box and clipped to the shape: sprites are a Rect + Texture. uSourceRect is in texels
// (x, y, w, h); zero width/height means the whole image. uFlip mirrors per axis
// (1 = flipped); SpriteSystem drives uSourceRect for animated sprites (a negative
// transform scale mirrors too).
//
// On lit layers the layer's normal and emission maps (MaterialLayer normalMap /
// emissionMap, or a sprite sheet's) are sampled at the same texel. Without a normal map,
// uBevel > 0 bends the normal outward within that many texels of the alpha edge: a
// cut-paper look. uNormalGreenDown flips DirectX-convention maps.
uniform vec2 e_TextureSize;     // texels; engine-fed when the layer binds a texture
uniform vec4 uTint; // default: 1 1 1 1
uniform vec4 uSourceRect; // default: 0 0 0 0
uniform vec2 uFlip; // default: 0 0
uniform float uBevel; // default: 0
uniform bool uNormalGreenDown; // default: false
uniform float uEmission; // default: 1

vec4 SourceRect()
{
    vec2 texSize = max(e_TextureSize, vec2(1.0));
    return (uSourceRect.z > 0.0 && uSourceRect.w > 0.0) ? uSourceRect : vec4(0.0, 0.0, texSize);
}

// uv -> texel inside the frame. Inset half a texel so bilinear filtering never bleeds in
// the neighbouring frame.
vec2 TexelOf(vec2 uv)
{
    vec2 st = clamp(uv, 0.0, 1.0);
    st = mix(st, 1.0 - st, step(0.5, uFlip));
    vec4 src = SourceRect();
    return clamp(src.xy + st * src.zw, src.xy + 0.5, src.xy + src.zw - 0.5);
}

vec2 TexUV(vec2 texel) { return texel / max(e_TextureSize, vec2(1.0)); }

vec4 Shade(float sd, vec2 p, vec2 uv)
{
    vec4 color = texture(texture0, TexUV(TexelOf(uv))) * uTint;
    return vec4(color.rgb, color.a * Coverage(sd));
}

float AlphaAt(vec2 texel)
{
    vec4 src = SourceRect();
    texel = clamp(texel, src.xy + 0.5, src.xy + src.zw - 0.5);
    return texture(texture0, TexUV(texel)).a;
}

#define HAS_SURFACE_NORMAL
vec3 SurfaceNormal(float sd, vec2 p, vec2 uv, vec4 color)
{
    // Image space is y-down and mirrored by uFlip; normals are y-up, in the sprite's frame.
    vec2 mirror = mix(vec2(1.0), vec2(-1.0), step(0.5, uFlip));
    vec2 texel = TexelOf(uv);
    if (e_HasNormalMap != 0)
    {
        vec3 n = texture(e_NormalMap, TexUV(texel)).rgb * 2.0 - 1.0;
        if (uNormalGreenDown) n.y = -n.y;
        return vec3(n.xy * mirror, n.z);
    }
    if (uBevel <= 0.0) return vec3(0.0, 0.0, 1.0);
    // Alpha rises inward, so its negated gradient points out of the silhouette.
    float gx = AlphaAt(texel + vec2(uBevel, 0.0)) - AlphaAt(texel - vec2(uBevel, 0.0));
    float gy = AlphaAt(texel + vec2(0.0, uBevel)) - AlphaAt(texel - vec2(0.0, uBevel));
    return normalize(vec3(vec2(-gx, gy) * 1.5 * mirror, 1.0));
}

#define HAS_SURFACE_EMISSION
vec3 SurfaceEmission(float sd, vec2 p, vec2 uv, vec4 color)
{
    if (e_HasEmissionMap == 0) return vec3(0.0);
    return texture(e_EmissionMap, TexUV(TexelOf(uv))).rgb * uEmission;
}
