
// SDF port of FireBorder.fs. The silhouette version estimates distance-to-edge by
// blurring texture0 with 24 taps per pixel; here sd *is* that distance, exact and free,
// so the whole effect is a falloff on sd with noise pushing the reach around.
uniform vec3 uGlowColor; // default: 1.0 0.42 0.05
uniform float uGlowRadius; // default: 14
uniform float uIntensity; // default: 1.2
uniform float uRise; // default: 0.8
// How much the fire fills the shape it burns around: 0 leaves the inside hollow (a burning
// outline), 1 fills it with the white-hot core.
uniform float uInteriorFade; // default: 0

float Flicker(float angle, float time)
{
    return sin(angle * 4.0  + time * 3.0) * 0.5
         + sin(angle * 7.0  - time * 5.0) * 0.25
         + sin(angle * 13.0 + time * 9.0) * 0.125;
}

vec4 Shade(float sd, vec2 p, vec2 uv)
{
    float angle = atan(p.y, p.x);

    // Flames climb: noise scrolls upward (screen y is down) and the reach is longer on
    // the top side of the shape.
    float n = Fbm(p * 0.035 + vec2(0.0, e_Time * 1.6));
    float up = max(-p.y / max(length(p), 1e-3), 0.0);
    float reach = uGlowRadius * (1.0 + 0.35 * Flicker(angle, e_Time))
                              * (0.55 + 0.9 * n)
                              * (1.0 + uRise * up);

    float d = max(sd, 0.0);
    float glow = exp(-d / max(reach, 1e-3));
    float core = exp(-d / max(reach * 0.25, 1e-3));   // white-hot band hugging the edge

    float pulse = 0.85 + 0.15 * sin(e_Time * 2.2);
    vec3 color = mix(uGlowColor, vec3(1.0, 0.93, 0.7), core);
    float interior = clamp(uInteriorFade, 0.0, 1.0);
    float alpha = clamp(glow * uIntensity * pulse, 0.0, 1.0) * mix(1.0 - Coverage(sd), 1.0, interior);
    return vec4(color, alpha);
}
