
// Northern lights: several ribbon layers whose lower edge waves on fbm, lit by
// vertical rays that shimmer over time, above layered mountain ridges and a still lake
// that mirrors the sky. uv lays out the scene, sd softly vignettes the frame.
uniform vec4 uSkyTop; // default: 0.0 0.01 0.04 1
uniform vec4 uSkyHorizon; // default: 0.02 0.07 0.13 1
uniform vec4 uAuroraLow; // default: 0.1 1 0.55 1
uniform vec4 uAuroraHigh; // default: 0.55 0.2 1 1
uniform float uIntensity; // default: 1.1
uniform float uSpeed; // default: 0.08
uniform float uLake; // default: 0.78

vec3 Sky(vec2 p, vec2 uv)
{
    float t = e_Time * uSpeed;
    vec3 color = mix(uSkyHorizon.rgb, uSkyTop.rgb, pow(clamp(1.0 - uv.y / uLake, 0.0, 1.0), 0.8));
    color += Starfield(p, 34.0) * 0.9 + Starfield(p + 13.0, 19.0) * 0.5;

    vec3 aurora = vec3(0.0);
    for (int i = 0; i < 4; i++)
    {
        float fi = float(i);
        float x = uv.x * (1.4 + fi * 0.35) + fi * 3.7;
        // Where this ribbon's lower edge sits, waving slowly.
        float base = 0.18 + fi * 0.07 + 0.16 * (Fbm(vec2(x * 1.3 + t * (1.0 + fi * 0.3), fi * 5.0)) - 0.5);
        float above = base - uv.y;                          // > 0 above the edge
        if (above < -0.02) continue;

        // Bright lower edge, long upward fade, vertical ray structure.
        float edge = smoothstep(-0.02, 0.01, above);
        float body = exp(-max(above, 0.0) / (0.10 + fi * 0.03));
        float rays = 0.45 + 0.55 * ValueNoise(vec2(x * 38.0, t * 6.0 + fi));
        rays *= 0.6 + 0.4 * ValueNoise(vec2(x * 90.0, t * 11.0 - fi));

        vec3 tint = mix(uAuroraLow.rgb, uAuroraHigh.rgb, clamp(above * 4.0 + fi * 0.12, 0.0, 1.0));
        aurora += tint * edge * body * rays * (0.9 - fi * 0.15);
    }
    return color + aurora * uIntensity;
}

vec4 Shade(float sd, vec2 p, vec2 uv)
{
    vec3 color;
    float aa = max(fwidth(uv.y), 1e-5);

    // Lake reflection: mirror uv about the shoreline, with a gentle ripple.
    bool inLake = uv.y > uLake;
    vec2 skyUv = uv;
    vec2 skyP = p;
    if (inLake)
    {
        float depth = uv.y - uLake;
        float ripple = sin(depth * 420.0 - e_Time * 1.5) * 0.0025 * (1.0 + depth * 8.0);
        skyUv = vec2(uv.x + ripple, uLake - depth);
        skyP = (skyUv - 0.5) * e_Size;
    }
    color = Sky(skyP, skyUv);

    // Mountains, three ridgelines receding into haze, reflected in the lake too.
    float ridgeY[3] = float[3](
        uLake - 0.20 * Fbm(vec2(skyUv.x * 2.5, 1.0)) - 0.02,
        uLake - 0.14 * Fbm(vec2(skyUv.x * 4.0 + 5.0, 2.0)) - 0.01,
        uLake - 0.07 * Fbm(vec2(skyUv.x * 7.0 + 9.0, 3.0)));
    vec3 ridgeColor[3] = vec3[3](vec3(0.04, 0.08, 0.13), vec3(0.025, 0.05, 0.08), vec3(0.01, 0.02, 0.035));
    for (int i = 0; i < 3; i++)
    {
        float m = clamp((skyUv.y - ridgeY[i]) / aa, 0.0, 1.0);
        color = mix(color, ridgeColor[i], m);
    }

    if (inLake)
    {
        color *= vec3(0.55, 0.7, 0.8);                      // water absorbs
        color += vec3(0.6, 0.9, 1.0) * exp(-(uv.y - uLake) / aa * 0.6) * 0.25;  // shore glint
    }

    // Soft vignette driven by distance from the frame edge.
    color *= mix(0.5, 1.0, clamp(-sd / 260.0, 0.0, 1.0));
    return vec4(color, Coverage(sd));
}
