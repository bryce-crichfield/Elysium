
// Exponential falloff outside the shape. Needs MaterialComponent padding to have room
// to bleed into.
uniform vec4 uColor; // default: 1 0.6 0.2 1
uniform float uRadius; // default: 12
uniform float uIntensity; // default: 1
uniform float uPulse; // default: 0

vec4 Shade(float sd, vec2 p, vec2 uv)
{
    float falloff = exp(-max(sd, 0.0) / max(uRadius, 1e-3));
    float pulse = 1.0 + uPulse * sin(e_Time * 2.0);
    float a = clamp(falloff * uIntensity * pulse, 0.0, 1.0);
    return vec4(uColor.rgb, uColor.a * a);
}

// Lit layers: an effect, not a surface. It glows (uEmission scales how much stays bright
// in the dark) and leaves the normals of whatever it's drawn over alone.
uniform float uEmission; // default: 1
#define HAS_SURFACE_EMISSION
#define SURFACE_NO_NORMAL
vec3 SurfaceEmission(float sd, vec2 p, vec2 uv, vec4 color) { return color.rgb * uEmission; }
