
// Outline band uWidth wide. uAlign places it: -1 inside the edge, 0 centered on it,
// 1 outside.
uniform vec4 uColor; // default: 1 1 1 1
uniform float uWidth; // default: 1
uniform float uAlign; // default: -1

vec4 Shade(float sd, vec2 p, vec2 uv)
{
    float center = uAlign * uWidth * 0.5;
    float band = abs(sd - center) - uWidth * 0.5;
    return vec4(uColor.rgb, uColor.a * Coverage(band));
}
