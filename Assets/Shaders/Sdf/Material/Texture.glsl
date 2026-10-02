// An image, or a sub-image of one (a sprite-sheet frame), stretched over the shape's
// box and clipped to the shape: sprites are a Rect + Texture. uSourceRect is in texels
// (x, y, w, h); zero width/height means the whole image. uFlip mirrors per axis
// (1 = flipped); SpriteSystem drives uSourceRect for animated sprites (a negative
// transform scale mirrors too).
uniform vec2 e_TextureSize;     // texels; engine-fed when the layer binds a texture
uniform vec4 uTint; // default: 1 1 1 1
uniform vec4 uSourceRect; // default: 0 0 0 0
uniform vec2 uFlip; // default: 0 0

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
