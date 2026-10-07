uniform vec3 uAmbient;
uniform int uLightCount;
uniform vec3 uLightPos[16];
uniform vec3 uLightColor[16];
uniform float uLightRadius[16];
uniform float uLightVision[16];
uniform int uShadowCount;       // lights with a row in uShadowAtlas (0: no shadows)
uniform sampler2D uShadowAtlas;
uniform float uShadowRows;
uniform float uShadowTile;
uniform float uShadowBias;
uniform vec3 uSunDir;           // toward the sun (GL, unit)
uniform vec3 uSunColor;         // zero: no sun
uniform float uRim;
uniform vec3 uTowardCamera;     // orthographic: the same everywhere
uniform vec3 uEye;              // perspective: the camera
uniform float uPerspective;
uniform float uFog;             // 0 off .. 1 the unseen is hidden
uniform vec3 uFogColor;

// Must match ShadowAtlas.cpp's face table.
float ShadowDistance(int light, vec3 v, vec2 offset)
{
    vec3 a = abs(v);
    int face;
    vec3 forward, up;
    if (a.x >= a.y && a.x >= a.z) {
        face = v.x > 0.0 ? 0 : 1; forward = vec3(sign(v.x), 0.0, 0.0); up = vec3(0.0, 1.0, 0.0);
    } else if (a.y >= a.z) {
        face = v.y > 0.0 ? 2 : 3; forward = vec3(0.0, sign(v.y), 0.0); up = vec3(0.0, 0.0, 1.0);
    } else {
        face = v.z > 0.0 ? 4 : 5; forward = vec3(0.0, 0.0, sign(v.z)); up = vec3(0.0, 1.0, 0.0);
    }
    vec3 right = cross(forward, up);
    vec2 uv = vec2(dot(v, right), dot(v, up)) / dot(v, forward) * 0.5 + 0.5;
    float inset = 1.0 / uShadowTile;
    uv = clamp(uv + offset / uShadowTile, vec2(inset), vec2(1.0 - inset));
    vec2 st = vec2((float(face) + uv.x) / 6.0, (float(light) + uv.y) / uShadowRows);
    return textureLod(uShadowAtlas, st, 0.0).r;
}

// 0 in shadow .. 1 lit, softened over a few texels.
float Shadow(int light, vec3 fromLight, float radius)
{
    if (light >= uShadowCount) return 1.0;
    float d = length(fromLight) / radius - 0.004;
    float lit = 0.0;
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++)
            lit += d < ShadowDistance(light, fromLight, vec2(x, y) * 1.25) ? 1.0 : 0.0;
    return lit / 9.0;
}

// The light reaching p (GL) and, in seen, how clearly any vision light sees it. n is the
// surface's normal, or zero for a card, which takes light from any side.
vec3 Light3D(vec3 p, vec3 n, out float seen)
{
    bool card = dot(n, n) < 0.5;
    vec3 lifted = p + n * uShadowBias;
    vec3 total = uAmbient * (card ? 1.0 : 0.75 + 0.25 * n.y);
    total += uSunColor * (card ? 0.6 : max(dot(n, uSunDir), 0.0));
    seen = 0.0;
    for (int i = 0; i < 16; i++) {
        if (i >= uLightCount) break;
        vec3 toLight = uLightPos[i] - p;
        float d = length(toLight);
        float radius = max(uLightRadius[i], 1.0);
        if (d >= radius) continue;
        float facing = card ? 0.6 : max(dot(n, toLight / max(d, 0.001)), 0.0);
        bool eyes = uLightVision[i] > 0.5 && (card || dot(n, toLight) > 0.0);
        if (facing <= 0.0 && !eyes) continue;
        float shadow = Shadow(i, lifted - uLightPos[i], radius);
        if (eyes) seen = max(seen, shadow * (1.0 - smoothstep(0.8, 1.0, d / radius)));
        float falloff = 1.0 - d / radius;
        total += uLightColor[i] * falloff * falloff * facing * shadow;
    }
    return total;
}

vec3 Fogged(vec3 color, float seen) { return mix(color, uFogColor, uFog * (1.0 - seen)); }
