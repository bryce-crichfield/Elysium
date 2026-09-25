#version 330

// FireBorder — reads the silhouette drawn into texture0's alpha channel (the entity,
// rendered into its own padded offscreen buffer by RenderCompositor::RenderShadedEntity)
// and burns a soft glowing rim around its edge, scaled by distance from the silhouette.

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

// Engine-fed every frame — see RenderCompositor::RenderShadedEntity.
uniform float e_Time;
uniform vec2 e_Resolution;
uniform vec2 e_TexelSize;

// Per-instance overrides (ShaderComponent's <Uniform> children); each falls back to the
// default below when no override is present.
uniform float uGlowRadius; // default: 12.0
uniform float uIntensity; // default: 1.0
uniform vec3 uGlowColor; // default: 1.0 0.42 0.05

out vec4 finalColor;

// Per-pixel pseudo-random angle so the sample spiral below doesn't line up the same way
// on every pixel — without this the fixed sample pattern shows up as visible rings.
float Hash(vec2 p)
{
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

// Three sine layers at incommensurate frequencies/speeds, some running backwards in time —
// gives a wandering, non-repeating-looking flicker from `angle` (position around the rim)
// and `time`, instead of the whole rim pulsing in lockstep.
float Flicker(float angle, float time)
{
    return sin(angle * 4.0  + time * 3.0) * 0.5
         + sin(angle * 7.0  - time * 5.0) * 0.25
         + sin(angle * 13.0 + time * 9.0) * 0.125;
}

void main()
{
    vec4 texel = texture(texture0, fragTexCoord);

    // Samples are placed along a golden-angle spiral: radius grows linearly with sample
    // index while angle advances by a fixed irrational fraction of a turn, so the whole
    // disc is covered evenly with no repeating ring/sector structure to band on. Weight
    // falls off continuously (exp) instead of in discrete radius buckets — that discrete
    // bucketing was the source of the visible banding in the previous version.
    const int kSamples = 24;
    const float kGoldenAngle = 2.39996323; // radians
    float rotation = Hash(fragTexCoord * e_Resolution) * 6.28318530718;

    float weightedAlpha = 0.0;
    float weightSum = 0.0;
    for (int i = 0; i < kSamples; i++)
    {
        float t = (float(i) + 0.5) / float(kSamples);  // continuous 0..1 radius fraction
        float angle = float(i) * kGoldenAngle + rotation;

        // Distorts how far this sample reaches per angle+time, so the rim's edge licks
        // and wanders around the silhouette instead of forming a static uniform ring.
        float radiusMod = 1.0 + 0.4 * Flicker(angle, e_Time);
        vec2 offset = vec2(cos(angle), sin(angle)) * (t * uGlowRadius * radiusMod) * e_TexelSize;

        float weight = exp(-t * 3.0);
        weightedAlpha += texture(texture0, fragTexCoord + offset).a * weight;
        weightSum += weight;
    }
    float glow = clamp((weightedAlpha / max(weightSum, 0.0001)) * 2.2, 0.0, 1.0);
    glow = smoothstep(0.0, 1.0, glow);  // softens the remaining transition into the silhouette

    // A slower, broader pulse breathes the whole rim's brightness in and out on top of the
    // per-angle licking above.
    float pulse = 0.8 + 0.2 * sin(e_Time * 2.2);

    vec3 rim = uGlowColor * glow * uIntensity * pulse;
    vec3 color = texel.rgb + rim * (1.0 - texel.a);
    float alpha = max(texel.a, glow * uIntensity * pulse);

    finalColor = vec4(color, alpha) * colDiffuse * fragColor;
}
