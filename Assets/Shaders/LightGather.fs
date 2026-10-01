#version 330

// First half of a lit layer's lighting (RenderCompositor::RenderLit): how much light
// reaches each point of the ground, and from where. Runs over a low-resolution grid of
// the ground, the same region as the ground emission (texture0, mipmapped) and the
// occluder field, so all three share texcoords.
//
// From each point, rays march outward in kDirections directions, each a cone as wide as
// its share of the circle. Along it they pick up the emission (from a mip about as wide
// as the cone) as if every glowing texel were a light hovering e_Height above the ground.
// With e_Shadows, rays stop at occluders: a wall at least e_Height tall ends the ray; a
// shorter one only hides emitters far enough past it that the line to them dips under
// its top (its shadow ends). Steps are as long as the cone is wide, but never longer than
// the distance to the nearest occluder, so no wall is stepped over.
//
// e_Mode 0 writes the light a flat surface facing the camera receives (rgb).
// e_Mode 1 writes the direction it mostly comes from (xyz, y-up, weighted by brightness).
// Lighting.fs combines the two with each pixel's normal at full resolution.

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform sampler2D e_OccluderMask;   // r * 2040 = the height of the occluder covering a texel
uniform sampler2D e_OccluderField;  // xy = nearest occluder texel, b = 1 if there is one
uniform vec2 e_GroundSize;          // world units
uniform vec2 e_GridSize;            // texels of the emission / occluder grid
uniform float e_Reach;              // world units
uniform float e_Height;             // world units
uniform float e_Strength;
uniform int e_Shadows;
uniform int e_Mode;

out vec4 finalColor;

const int kDirections = 24;
const int kMaxSteps = 72;
const float kPi = 3.14159265;
const float kMaskScale = 2040.0;    // see RenderCompositor::BuildOccluderField

float OccluderHeight(vec2 st)
{
    return textureLod(e_OccluderMask, st, 0.0).r * kMaskScale;
}

// World units to the nearest occluder texel.
float OccluderDistance(vec2 st, float texelWorld)
{
    ivec2 texel = clamp(ivec2(st * e_GridSize), ivec2(0), ivec2(e_GridSize) - 1);
    vec4 seed = texelFetch(e_OccluderField, texel, 0);
    if (seed.b < 0.5) return 1e6;
    return distance(seed.xy, vec2(texel)) * texelWorld;
}

void main()
{
    vec2 origin = fragTexCoord;
    float texelWorld = e_GroundSize.x / e_GridSize.x;
    float height = max(e_Height, 1.0);
    float reach = max(e_Reach, 1.0);
    float cone = 2.0 * kPi / float(kDirections);
    bool shadows = e_Shadows != 0;
    // A point under an occluder (a wall's own base, a unit's feet) first walks out of it:
    // the wall doesn't shadow itself.
    bool startsInside = shadows && OccluderHeight(origin) > 0.0;

    vec3 light = vec3(0.0);
    vec3 direction = vec3(0.0);
    for (int d = 0; d < kDirections; d++)
    {
        float angle = (float(d) + 0.5) * cone;
        vec2 dir = vec2(cos(angle), sin(angle));            // world, y-down
        vec2 dirSt = vec2(dir.x, -dir.y) / e_GroundSize;    // per world unit, in texcoords
        vec3 toLight = normalize(vec3(dir.x, -dir.y, 0.0)); // y-up, before the height

        float t = texelWorld;
        float end = reach;          // shortened by occluders the ray passes
        bool escaping = startsInside;
        for (int i = 0; i < kMaxSteps && t < end; i++)
        {
            vec2 st = origin + dirSt * t;
            if (any(lessThan(st, vec2(0.0))) || any(greaterThan(st, vec2(1.0)))) break;

            float width = t * cone;
            float stepLength = max(width, texelWorld);
            bool stop = false;
            if (shadows)
            {
                float occluder = OccluderHeight(st);
                if (escaping)
                {
                    if (occluder > 0.0) { t += texelWorld; continue; }
                    escaping = false;
                }
                if (occluder >= height)
                {
                    // A wall: its own face may glow (a torch reprojected onto its base),
                    // so take that, then nothing past it.
                    stop = true;
                    stepLength = texelWorld * 2.0;
                }
                else if (occluder > 0.0)
                {
                    // Low enough to see over: emitters beyond t * height / occluder are
                    // behind its top from here. Walk through it.
                    end = min(end, t * height / occluder);
                    stepLength = texelWorld;
                }
                else
                {
                    stepLength = min(stepLength, max(OccluderDistance(st, texelWorld), texelWorld));
                }
            }

            // Every glowing texel is a light `height` above the ground: its light falls
            // off with the cube of the distance (inverse square, times the cosine at a
            // surface facing up), and the cone covers t * cone * stepLength of ground.
            // Over a uniform glow, one direction's weights add up to 1.
            float lod = log2(max(width / texelWorld, 1.0));
            vec3 emission = textureLod(texture0, st, lod).rgb;
            float dist2 = t * t + height * height;
            float weight = stepLength * t * height / (dist2 * sqrt(dist2));
            weight *= 1.0 - smoothstep(0.6 * reach, reach, t);
            vec3 tap = emission * weight;
            light += tap;
            direction += normalize(vec3(toLight.xy * t, height)) * dot(tap, vec3(0.299, 0.587, 0.114));

            if (stop) break;
            t += stepLength;
        }
    }
    light *= e_Strength * (2.5 / float(kDirections));
    finalColor = e_Mode == 0 ? vec4(light, 1.0) : vec4(direction, 1.0);
}
