// Glitter: a soft highlight sweeping across the shape as its direction turns, under white
// glints that wink on and off, each a turning four-point star. Masked by the layer's texture
// alpha when it binds one (put the gem's image on it too), so it only shimmers on the picture;
// glints may spill uSpill world units past the shape. Meant to sit over a Texture layer.
// uTint colors it all; uIntensity 0 turns it off.
uniform float uIntensity; // default: 1
uniform float uSheen; // default: 0.3
uniform float uSheenScale; // default: 0.15
uniform float uSpin; // default: 0.8
uniform float uCellSize; // default: 3.5
uniform float uDensity; // default: 0.45
uniform float uTwinkle; // default: 3
uniform float uSize; // default: 1.2
uniform float uSpill; // default: 2
uniform vec4 uTint; // default: 1 1 1 1

float Mask(vec2 uv)
{
    vec2 st = clamp(uv, 0.0, 1.0);
    float inBox = step(0.0, uv.x) * step(uv.x, 1.0) * step(0.0, uv.y) * step(uv.y, 1.0);
    return texture(texture0, st).a * inBox;
}

// One glint: a four-point star turned by `angle`, its arms `len` long.
float Star(vec2 d, float angle, float len)
{
    float c = cos(angle), s = sin(angle);
    d = vec2(c * d.x - s * d.y, s * d.x + c * d.y);
    float arms = exp(-abs(d.x) / (0.12 * len)) * exp(-abs(d.y) / len)
               + exp(-abs(d.y) / (0.12 * len)) * exp(-abs(d.x) / len);
    float core = exp(-dot(d, d) / (0.08 * len * len));
    return arms * 0.7 + core;
}

vec4 Shade(float sd, vec2 p, vec2 uv)
{
    if (uIntensity <= 0.0) return vec4(0.0);
    float mask = Mask(uv) * Coverage(sd);

    // The sheen: highlight bands across a direction that keeps turning, rippled a little.
    vec2 dir = vec2(cos(e_Time * uSpin), sin(e_Time * uSpin));
    float band = dot(p, dir) * uSheenScale + ValueNoise(p * 0.3 + e_Time * 0.5) * 0.4 - e_Time * 0.3;
    float sheen = pow(0.5 + 0.5 * sin(band * 6.2831853), 4.0);

    // The glints, one per lit cell of a jittered grid (the 3x3 around, so arms cross cells).
    float glint = 0.0;
    vec2 cell = floor(p / uCellSize);
    for (int y = -1; y <= 1; ++y)
    for (int x = -1; x <= 1; ++x)
    {
        vec2 c = cell + vec2(x, y);
        float h = Hash21(c);
        if (h > uDensity) continue;
        vec2 at = (c + 0.5 + (vec2(Hash21(c + 3.1), Hash21(c + 7.7)) - 0.5) * 0.8) * uCellSize;
        float phase = Hash21(c + 1.3) * 6.2831853;
        float rate = uTwinkle * (0.6 + 0.8 * Hash21(c + 5.9));
        float wink = pow(max(0.0, sin(e_Time * rate + phase)), 6.0);
        float spin = e_Time * (Hash21(c + 2.2) - 0.5) * 3.0 + phase;
        glint += Star(p - at, spin, uCellSize * uSize * (0.6 + 0.6 * wink)) * wink;
    }
    float spill = Coverage(sd - uSpill);
    glint = clamp(glint, 0.0, 1.0) * max(mask, spill * 0.6);

    float a = clamp(uSheen * sheen * mask + glint, 0.0, 1.0);
    return vec4(uTint.rgb, a * uTint.a * clamp(uIntensity, 0.0, 1.0));
}
