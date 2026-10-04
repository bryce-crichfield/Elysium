// A pop: sparks fly out from the shape's edge and burn out, with a flash at the start. One
// shot, driven by uProgress from 0 to 1 (nothing is drawn outside that). Each spark's angle,
// speed and size come from a hash of its index and uSeed, so a different seed is a different
// pop. Needs MaterialComponent padding of about uDistance for the sparks to fly into.
uniform vec4 uColor; // default: 0.4 0.7 1 1
uniform float uProgress; // default: 1
uniform float uDistance; // default: 30
uniform float uSparks; // default: 14
uniform float uSize; // default: 2.5
uniform float uFlash; // default: 1
uniform float uSeed; // default: 0

const int MAX_SPARKS = 32;

vec4 Shade(float sd, vec2 p, vec2 uv)
{
    float t = uProgress;
    if (t <= 0.0 || t >= 1.0) return vec4(0.0);

    // Fast out, easing to a stop.
    float travel = 1.0 - pow(1.0 - t, 3.0);
    float start = 0.5 * min(e_Size.x, e_Size.y);  // sparks leave from about the edge
    float glow = 0.0;
    float hot = 0.0;

    for (int i = 0; i < MAX_SPARKS; ++i)
    {
        if (float(i) >= uSparks) break;
        float fi = float(i) + uSeed * 17.0;
        float jitter = Hash21(vec2(fi, 1.7)) - 0.5;
        float angle = (float(i) + jitter * 0.8) / max(uSparks, 1.0) * 6.2831853;
        float speed = 0.55 + 0.45 * Hash21(vec2(fi, 9.3));
        float size = uSize * (0.6 + 0.8 * Hash21(vec2(fi, 4.1))) * (1.0 - t);

        vec2 dir = vec2(cos(angle), sin(angle));
        vec2 center = dir * (start + uDistance * speed * travel);
        // Stretched along its flight while it's fast: a short streak.
        vec2 d = p - center;
        float along = dot(d, dir);
        float across = dot(d, vec2(-dir.y, dir.x));
        float stretch = 1.0 + 2.5 * (1.0 - travel);
        float r = length(vec2(along / stretch, across));

        glow += exp(-r / max(size, 1e-3));
        hot += exp(-r / max(size * 0.35, 1e-3));
    }

    // A flash at the shape that's gone within the first quarter.
    float flash = uFlash * exp(-max(sd, 0.0) / max(start, 1.0)) * max(0.0, 1.0 - t * 4.0);

    float fade = 1.0 - t * t;
    float alpha = clamp((glow + flash) * fade, 0.0, 1.0) * uColor.a;
    vec3 color = mix(uColor.rgb, vec3(1.0, 0.97, 0.9), clamp(hot + flash * 0.5, 0.0, 1.0));
    return vec4(color, alpha);
}
