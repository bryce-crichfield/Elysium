#version 330

// Pulse — tints the entity toward a color that fades in and out over time, keeping its
// alpha, so the silhouette glows gently. A small example of the shader conventions.

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

// Engine-fed every frame — see RenderCompositor::RenderShadedEntity.
uniform float e_Time;

// Per-instance overrides (ShaderComponent's <Uniform> children); each falls back to the
// default below when no override is present.
uniform vec3 uPulseColor; // default: 0.66 0.55 0.88
uniform float uSpeed; // default: 2.0
uniform float uStrength; // default: 0.5

out vec4 finalColor;

void main()
{
    vec4 texel = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    float pulse = (sin(e_Time * uSpeed) * 0.5 + 0.5) * uStrength;
    finalColor = vec4(mix(texel.rgb, uPulseColor, pulse), texel.a);
}

