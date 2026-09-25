
// Retro outrun backdrop: a striped sun sinking behind fbm mountains, over an infinite
// neon perspective grid scrolling toward the viewer. uv drives the layout (so it fills
// any box), p (world units) keeps circles round, sd adds the neon frame edge.
uniform vec4 uSkyTop; // default: 0.04 0.01 0.12 1
uniform vec4 uSkyHorizon; // default: 0.55 0.08 0.45 1
uniform vec4 uSunTop; // default: 1 0.9 0.3 1
uniform vec4 uSunBottom; // default: 1 0.15 0.55 1
uniform vec4 uGridColor; // default: 0.2 0.9 1 1
uniform float uHorizon; // default: 0.6
uniform float uSpeed; // default: 0.6

vec4 Shade(float sd, vec2 p, vec2 uv)
{
    float aspect = e_Size.x / max(e_Size.y, 1.0);
    float horizonY = (uHorizon - 0.5) * e_Size.y;          // in p space (y down)
    float h = uv.y - uHorizon;                              // < 0 above the horizon

    vec3 color;
    if (h < 0.0)
    {
        // Sky: gradient, stars fading in toward the top.
        float k = clamp(-h / uHorizon, 0.0, 1.0);
        color = mix(uSkyHorizon.rgb, uSkyTop.rgb, pow(k, 0.6));
        color += Starfield(p + vec2(e_Time * 3.0, 0.0), 38.0) * smoothstep(0.25, 0.9, k);

        // Sun: disc with horizontal cut stripes that thicken and scroll down toward
        // the horizon.
        float radius = e_Size.y * 0.24;
        vec2 center = vec2(0.0, horizonY - radius * 0.35);
        float dSun = length(p - center) - radius;
        float sy = (p.y - center.y) / radius;               // -1 top .. 1 bottom
        float stripes = 1.0;
        if (sy > -0.1)
        {
            float band = fract(sy * 7.0 - e_Time * 0.35);
            float gap = mix(0.0, 0.55, clamp((sy + 0.1) / 1.1, 0.0, 1.0));
            stripes = smoothstep(gap - 0.03, gap + 0.03, band);
        }
        vec3 sun = mix(uSunTop.rgb, uSunBottom.rgb, clamp(sy * 0.5 + 0.5, 0.0, 1.0));
        float sunMask = Coverage(dSun) * stripes;
        color += uSunBottom.rgb * exp(-max(dSun, 0.0) / (radius * 0.45)) * 0.55;  // bloom
        color = mix(color, sun, sunMask);

        // Mountains: two fbm ridgelines, the nearer one darker, cutting the sun.
        float x = p.x / e_Size.y;
        float ridgeFar  = horizonY - e_Size.y * (0.05 + 0.10 * Fbm(vec2(x * 3.0, 1.0)));
        float ridgeNear = horizonY - e_Size.y * (0.02 + 0.07 * Fbm(vec2(x * 5.0 + 7.0, 3.0)));
        float aa = max(fwidth(p.y), 1e-3);
        color = mix(color, vec3(0.13, 0.03, 0.2), clamp((p.y - ridgeFar) / aa, 0.0, 1.0));
        color = mix(color, vec3(0.05, 0.01, 0.09), clamp((p.y - ridgeNear) / aa, 0.0, 1.0));
        // Neon rim along the far ridge.
        color += uGridColor.rgb * exp(-abs(p.y - ridgeFar) / 1.5) * 0.5;
    }
    else
    {
        // Floor: project back onto a ground plane. depth grows toward the horizon.
        float depth = 0.12 / max(h, 1e-4);
        vec2 ground = vec2((uv.x - 0.5) * aspect * depth, depth + e_Time * uSpeed);
        vec2 cell = abs(fract(ground * 4.0) - 0.5);
        vec2 width = fwidth(ground * 4.0) * 1.4;
        vec2 lines = 1.0 - smoothstep(vec2(0.0), width, 0.5 - cell);
        float grid = max(lines.x, lines.y);

        float fade = exp(-depth * 0.35);
        color = mix(vec3(0.03, 0.0, 0.07), uSkyHorizon.rgb * 0.35, exp(-h * 9.0));
        color += uGridColor.rgb * grid * fade * 1.4;
        color += uGridColor.rgb * exp(-h * 40.0) * 0.6;    // horizon haze line
    }

    // Scanlines + neon frame.
    color *= 0.92 + 0.08 * sin(p.y * 1.6);
    float aaEdge = max(fwidth(sd), 1e-4);
    color += uGridColor.rgb * exp(-abs(sd) / (aaEdge * 1.5));

    return vec4(color, Coverage(sd));
}
