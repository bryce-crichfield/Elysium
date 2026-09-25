
// Burn-away fill: the shape is eaten by a noise threshold, with a glowing band where it
// burns. Use it in place of Flat (it draws the fill itself) and drive uProgress 0 -> 1
// for spawn/death/teleport. uAutoSpeed > 0 ping-pongs progress on its own (demo/idle).
uniform vec4 uColor; // default: 1 1 1 1
uniform float uProgress; // default: 0.5
uniform float uAutoSpeed; // default: 0
uniform vec3 uEdgeColor; // default: 1 0.45 0.1
uniform float uEdgeWidth; // default: 0.08
uniform float uNoiseScale; // default: 0.04

vec4 Shade(float sd, vec2 p, vec2 uv)
{
    float progress = uAutoSpeed > 0.0 ? 0.5 - 0.5 * cos(e_Time * uAutoSpeed) : uProgress;

    // fbm lands roughly in 0.1..0.9; widen the threshold range so 0 and 1 are truly
    // fully-present and fully-gone.
    float n = Fbm(p * uNoiseScale + 3.7);
    float threshold = mix(-0.05, 1.05 + uEdgeWidth, progress);
    float edge = n - threshold;                              // > 0 survives

    float aa = max(fwidth(n), 1e-4);
    float alive = clamp(edge / aa + 0.5, 0.0, 1.0);
    float burn = 1.0 - smoothstep(0.0, uEdgeWidth, edge);   // 1 right at the burn front

    vec4 base = uColor;
    vec3 hot = mix(uEdgeColor, vec3(1.0, 0.95, 0.7), burn * burn);
    vec3 color = mix(base.rgb, hot * 1.4, burn);
    float alpha = mix(base.a, 1.0, burn) * alive;
    return vec4(color, alpha * Coverage(sd));
}
