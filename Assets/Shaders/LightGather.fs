#version 330

// First half of a lit layer's lighting (RenderCompositor::RenderLit), run at a fraction of
// the layer's resolution because light from emitters is smooth. texture0 is the layer's
// emission buffer, mipmapped. Every glowing pixel is treated as a light hovering e_Height
// above the layer: nearby emission is sampled sharp, and further out the mips stand in for
// whole regions of it, so a wide glow and a small hot spot both carry to e_Reach.
//
// e_Mode 0 writes the light a flat surface facing the camera receives (rgb).
// e_Mode 1 writes the direction it mostly comes from (xyz, y-up, weighted by brightness).
// Lighting.fs combines the two with each pixel's normal at full resolution.

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform vec2 e_Resolution;  // the layer's full resolution; offsets are in its pixels
uniform float e_Reach;      // pixels
uniform float e_Height;     // pixels
uniform float e_Strength;
uniform int e_Mode;

out vec4 finalColor;

const int kRings = 6;       // radii double from e_Reach / 2^(kRings-1) out to e_Reach
const int kDirections = 10;
const float kPi = 3.14159265;

void main()
{
    vec3 light = vec3(0.0);
    vec3 direction = vec3(0.0);
    float height = max(e_Height, 1.0);
    for (int ring = 0; ring < kRings; ring++)
    {
        float radius = e_Reach * exp2(float(ring - kRings + 1));
        // Each tap stands for an arc of the ring about as wide as the ring is deep, so
        // sample a mip whose texels are about that big.
        float footprint = radius * 2.0 * kPi / float(kDirections);
        float lod = log2(max(footprint * 0.5, 1.0));
        // A tap's light falls off with the square of its distance; taps further out cover
        // more area, which cancels, until the height takes over up close.
        float weight = (radius * radius) / (radius * radius + height * height);
        weight *= 1.0 - smoothstep(0.6, 1.0, radius / max(e_Reach, 1.0));
        for (int d = 0; d < kDirections; d++)
        {
            float angle = (float(d) + 0.5 * float(ring & 1)) * 2.0 * kPi / float(kDirections);
            vec2 offset = vec2(cos(angle), sin(angle)) * radius;        // pixels, y-down
            vec2 st = fragTexCoord + vec2(offset.x, -offset.y) / e_Resolution;
            vec3 emission = textureLod(texture0, clamp(st, 0.0, 1.0), lod).rgb;
            vec3 toEmitter = normalize(vec3(offset.x, -offset.y, height)); // y-up
            vec3 tap = emission * weight;
            light += tap * toEmitter.z;
            direction += toEmitter * dot(tap, vec3(0.299, 0.587, 0.114));
        }
    }
    light *= e_Strength * (2.0 / float(kDirections));
    finalColor = e_Mode == 0 ? vec4(light, 1.0) : vec4(direction, 1.0);
}
