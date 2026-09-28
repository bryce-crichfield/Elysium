
// Electric / static border (ported from the "Radiant border" electric style). The arc
// pattern is fbm sampled in a frame that jumps to a new random rotation + offset at
// random intervals, so the discharge snaps between shapes instead of flowing. Interior
// stays transparent (fades inward over uInteriorFade), so it layers safely over a Flat
// fill or any content. Give the MaterialComponent generous padding — the outer glow is
// wide — Main.glsl fades it out at the quad edge.
uniform vec3 uColor; // default: 0.541 0.184 0.878
uniform float uGlowWidth; // default: 90
uniform float uIntensity; // default: 1.2
uniform float uPulseSpeed; // default: 0.7
uniform float uTurbulence; // default: 5.5
uniform float uInteriorFade; // default: 90
uniform float uJumpMin; // default: 0
uniform float uJumpMax; // default: 0.075
uniform float uJumpSkew; // default: 4.5

// Stateless random-interval clock, O(1) and always advancing. (The original walked
// forward through random gaps with a 220-step cap; whenever a cycle's gaps came out
// short the walk hit the cap before reaching `time` and the pattern froze until the
// cycle wrapped.) Time is cut into slots of uJumpMax. Each slot either holds one
// pattern for its whole length — the occasional long "freeze", chance 1/(skew+1), so
// higher skew means rarer holds — or is split into a random number of fast ticks no
// shorter than uJumpMin (and never shorter than a frame, which would be invisible).
vec2 JumpSeed(float time)
{
    float slotLen = max(uJumpMax, 0.005);
    float fastLen = clamp(max(uJumpMin, 1.0 / 60.0), 0.001, slotLen);
    float slot = floor(time / slotLen);
    float within = fract(time / slotLen);

    float h = Hash21(vec2(slot, 7.3));
    if (h < 1.0 / (uJumpSkew + 1.0)) return vec2(slot, 0.0);  // hold

    float maxTicks = max(floor(slotLen / fastLen), 1.0);
    float ticks = 1.0 + floor(Hash21(vec2(slot, 2.9)) * maxTicks);
    return vec2(slot, 1.0 + floor(within * ticks));
}

vec4 Shade(float sd, vec2 p, vec2 uv)
{
    vec2 seed = JumpSeed(e_Time);

    float ang = Hash21(seed) * 6.2831853;
    vec2 jump = (vec2(Hash21(seed + vec2(3.1, 7.7)), Hash21(seed + vec2(9.3, 2.2))) * 2.0 - 1.0) * 4.0;
    mat2 rot = mat2(cos(ang), -sin(ang), sin(ang), cos(ang));
    float n = Fbm(rot * (p * 0.06) + jump);

    float jag = pow(1.0 - abs(n * 2.0 - 1.0), 6.0);          // thin ridges = arcs
    float strobe = Hash21(seed + vec2(0.0, 19.0));
    float flicker = (0.55 + 0.75 * jag) * (0.7 + 0.5 * strobe);
    float pulse = uPulseSpeed > 0.001 ? 0.82 + 0.18 * sin(e_Time * uPulseSpeed) : 1.0;

    float dd = sd + (n - 0.5) * uTurbulence * 0.5;          // noise-displaced edge

    float k = 60.0 / max(uGlowWidth, 1.0);
    float border = exp(-abs(dd) * k * 0.05);
    float outward = exp(-max(dd, 0.0) * k * 0.015);
    float glow = (border * 1.5 + outward * 0.55) * uIntensity * pulse * flicker;

    glow *= exp(-max(-dd, 0.0) / max(uInteriorFade, 1.0));  // transparent interior

    vec3 color = mix(uColor, vec3(1.0, 0.98, 1.0), clamp(border * 1.6, 0.0, 1.0));
    return vec4(color, clamp(glow, 0.0, 1.0));
}

// Lit layers: an effect, not a surface. It glows (uEmission scales how much stays bright
// in the dark) and leaves the normals of whatever it's drawn over alone.
uniform float uEmission; // default: 1
#define HAS_SURFACE_EMISSION
#define SURFACE_NO_NORMAL
vec3 SurfaceEmission(float sd, vec2 p, vec2 uv, vec4 color) { return color.rgb * uEmission; }
