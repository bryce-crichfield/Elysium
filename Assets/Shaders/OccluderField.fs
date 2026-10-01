#version 330

// Jump flood over the occluder mask (RenderCompositor::BuildOccluderField): turns the
// rasterized footprints into, for every texel of the ground grid, the texel of the
// nearest occluder. LightGather.fs reads the distance to it as how far a ray may step
// without passing through a wall.
//
// Each texel holds (x, y) of its nearest occluder texel in grid texels, and b = 1 once it
// knows one. e_Mode 0 seeds from the mask (texture0, alpha = covered); e_Mode 1 is one
// flood step, looking e_Step texels away in the 8 directions (texture0, the last step).

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform vec2 e_GridSize;    // texels
uniform int e_Mode;
uniform float e_Step;       // texels

out vec4 finalColor;

void main()
{
    ivec2 size = ivec2(e_GridSize);
    ivec2 texel = clamp(ivec2(fragTexCoord * e_GridSize), ivec2(0), size - 1);

    if (e_Mode == 0)
    {
        bool covered = texelFetch(texture0, texel, 0).a > 0.5;
        finalColor = covered ? vec4(vec2(texel), 1.0, 1.0) : vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    int step = int(e_Step);
    vec2 best = vec2(0.0);
    float bestDistance = 1e20;
    for (int dy = -1; dy <= 1; dy++)
    {
        for (int dx = -1; dx <= 1; dx++)
        {
            ivec2 q = texel + ivec2(dx, dy) * step;
            if (any(lessThan(q, ivec2(0))) || any(greaterThanEqual(q, size))) continue;
            vec4 seed = texelFetch(texture0, q, 0);
            if (seed.b < 0.5) continue;
            float d = distance(seed.xy, vec2(texel));
            if (d < bestDistance)
            {
                bestDistance = d;
                best = seed.xy;
            }
        }
    }
    // Alpha 1 either way: the pass is drawn alpha-blended, so this is a plain write.
    finalColor = bestDistance < 1e19 ? vec4(best, 1.0, 1.0) : vec4(0.0, 0.0, 0.0, 1.0);
}
