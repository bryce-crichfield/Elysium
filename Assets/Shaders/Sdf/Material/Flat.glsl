
// Solid fill.
uniform vec4 uColor; // default: 1 1 1 1

vec4 Shade(float sd, vec2 p, vec2 uv)
{
    return vec4(uColor.rgb, uColor.a * Coverage(sd));
}

// Lit layers: 0 is a lit surface, 1 glows at full color regardless of light.
uniform float uEmission; // default: 0
#define HAS_SURFACE_EMISSION
vec3 SurfaceEmission(float sd, vec2 p, vec2 uv, vec4 color) { return color.rgb * uEmission; }
