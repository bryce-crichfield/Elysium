
// Full-frame animated backdrop: a domain-warped fbm nebula (Inigo Quilez's f(p + f(p +
// f(p))) warp) with a twinkling starfield, a hot rim line on the shape's edge and a
// vignette toward it. Put it on a big rectangle in a screen-space layer.
uniform vec4 uDeep; // default: 0.02 0.01 0.06 1
uniform vec4 uColorA; // default: 0.08 0.55 0.85 1
uniform vec4 uColorB; // default: 0.85 0.15 0.55 1
uniform vec4 uRimColor; // default: 0.55 0.9 1 1
uniform float uScale; // default: 0.0028
uniform float uSpeed; // default: 0.04
uniform float uStars; // default: 1

vec4 Shade(float sd, vec2 p, vec2 uv)
{
    float t = e_Time * uSpeed;
    vec2 q0 = p * uScale;

    // Two rounds of domain warp; q and r double as colour drivers below.
    vec2 q = vec2(Fbm(q0 + vec2(0.0, 0.0) + t),
                  Fbm(q0 + vec2(5.2, 1.3) - t));
    vec2 r = vec2(Fbm(q0 + 4.0 * q + vec2(1.7, 9.2) + 1.6 * t),
                  Fbm(q0 + 4.0 * q + vec2(8.3, 2.8) - 1.2 * t));
    float f = Fbm(q0 + 4.0 * r);

    vec3 color = uDeep.rgb;
    color = mix(color, uColorA.rgb, clamp(f * f * 2.2, 0.0, 1.0));
    color = mix(color, uColorB.rgb, clamp(length(q) * 0.9 - 0.35, 0.0, 1.0) * 0.8);
    color = mix(color, vec3(1.0, 0.95, 0.9), clamp(r.x * r.x * f * 1.4 - 0.25, 0.0, 1.0));
    color *= 0.35 + 1.1 * f;                              // dust lanes

    // Parallax starfield: two layers drifting at different rates, dimmed by dense gas.
    float gas = clamp(f * 1.4, 0.0, 1.0);
    color += uStars * (Starfield(p + vec2(e_Time * 4.0, 0.0), 42.0) +
                       Starfield(p + vec2(e_Time * 9.0, 11.0), 23.0) * 0.6) * (1.0 - gas * 0.7);

    // Hot rim line on the edge itself.
    float aa = max(fwidth(sd), 1e-4);
    float depth = max(-sd, 0.0);
    color += uRimColor.rgb * (exp(-abs(sd) / (aa * 1.5)) + 0.35 * exp(-depth / 12.0));

    // Subtle vignette toward the edge.
    color *= mix(0.55, 1.0, clamp(depth / 320.0, 0.0, 1.0));

    return vec4(color, Coverage(sd));
}
