
// Rings that leave the shape's edge and expand outward, fading as they go: attack
// telegraphs, threatened tiles, pings. Rings follow the geometry's outline (round on a
// circle, rounded-square on a rect). Keep uReach below the MaterialComponent padding;
// Main.glsl fades whatever reaches the quad edge.
uniform vec4 uColor; // default: 1 0.3 0.25 1
uniform float uCount; // default: 3
uniform float uReach; // default: 60
uniform float uWidth; // default: 2.5
uniform float uSpeed; // default: 0.6
uniform float uInnerGlow; // default: 0.35

vec4 Shade(float sd, vec2 p, vec2 uv)
{
    float intensity = 0.0;
    int count = int(clamp(uCount, 1.0, 8.0));
    for (int i = 0; i < 8; i++)
    {
        if (i >= count) break;
        float age = fract(e_Time * uSpeed + float(i) / float(count));  // 0 born .. 1 gone
        float fade = (1.0 - age) * (1.0 - age);
        intensity += exp(-abs(sd - age * uReach) / max(uWidth, 0.1)) * fade;
    }

    // A soft glow hugging the edge ties the rings to the shape.
    intensity += uInnerGlow * exp(-abs(sd) / 6.0);

    return vec4(uColor.rgb, uColor.a * clamp(intensity, 0.0, 1.0));
}
