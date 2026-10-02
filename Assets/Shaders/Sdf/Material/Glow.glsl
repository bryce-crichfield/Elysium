
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
