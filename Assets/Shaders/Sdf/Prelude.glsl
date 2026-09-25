#version 330

// SDF shader prelude. A composed SDF shader is Prelude + Geometry/<g>.glsl +
// Material/<m>.glsl + Main.glsl, assembled by ShaderAsset from a key path like
// "Shaders/Sdf/RoundedRect+Stroke.sdf" (see Core/Assets/ShaderAsset.cpp).
//
// Contract:
//   Geometry defines  float SceneDistance(vec2 p)
//     p is in world units, centered on the shape. Negative inside, positive outside.
//   Material defines  vec4 Shade(float sd, vec2 p, vec2 uv)
//     uv is 0..1 across the shape's box (not the padded quad). Returns straight alpha.
//
// e_ uniforms are engine-fed per draw (RenderCompositor::RenderMaterialEntity) and are
// never exposed as per-layer overrides.

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform float e_Time;
uniform vec2  e_Size;          // the shape's box, world units
uniform vec2  e_QuadSize;      // e_Size plus padding on every side
uniform float e_CornerRadius;  // world units
uniform vec2  e_PointA;        // segment endpoints, relative to the box center
uniform vec2  e_PointB;
uniform float e_Thickness;

out vec4 finalColor;

// Anti-aliased coverage of the region sd < 0: one screen pixel wide ramp.
float Coverage(float sd)
{
    float aa = max(fwidth(sd), 1e-4);
    return clamp(0.5 - sd / aa, 0.0, 1.0);
}

// ---- Noise helpers (unused ones compile away) --------------------------------------

float Hash21(vec2 p)
{
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float ValueNoise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(Hash21(i),                  Hash21(i + vec2(1.0, 0.0)), u.x),
               mix(Hash21(i + vec2(0.0, 1.0)), Hash21(i + vec2(1.0, 1.0)), u.x), u.y);
}

// 5-octave fractal noise, each octave rotated so the lattice never lines up.
float Fbm(vec2 p)
{
    const mat2 rot = mat2(0.80, 0.60, -0.60, 0.80);
    float sum = 0.0;
    float amp = 0.5;
    for (int i = 0; i < 5; i++)
    {
        sum += amp * ValueNoise(p);
        p = rot * p * 2.03 + vec2(17.0, 9.2);
        amp *= 0.5;
    }
    return sum;
}

// Sparse twinkling stars on a jittered grid of cellSize world units.
vec3 Starfield(vec2 p, float cellSize)
{
    vec2 cell = floor(p / cellSize);
    vec2 local = fract(p / cellSize) - 0.5;
    float h = Hash21(cell);
    if (h < 0.86) return vec3(0.0);                      // most cells are empty
    vec2 offset = vec2(Hash21(cell + 3.1), Hash21(cell + 7.7)) - 0.5;
    float d = length(local - offset * 0.7);
    float twinkle = 0.55 + 0.45 * sin(e_Time * (1.5 + h * 4.0) + h * 40.0);
    float star = exp(-d * d * 900.0) + 0.25 * exp(-d * 55.0);
    return vec3(0.85, 0.9, 1.0) * star * twinkle;
}
