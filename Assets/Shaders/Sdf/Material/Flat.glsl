
// Solid fill.
uniform vec4 uColor; // default: 1 1 1 1

vec4 Shade(float sd, vec2 p, vec2 uv)
{
    return vec4(uColor.rgb, uColor.a * Coverage(sd));
}
