#version 330

// Moves a lit layer's emission down onto the ground (RenderCompositor::RenderLit). The
// emission buffer (texture0) is where glowing pixels are drawn; light has to leave from
// where they stand. A torch drawn on a wall's face stands on the wall's ground line, a
// flame on its base. e_HeightBuffer says how far above its ground each pixel is drawn.
//
// Each ground texel looks up its column (y-up is y-minus in the world) for pixels whose
// height puts them right here. Samples are spaced one ground texel apart, and each stands
// for that much of the column, so summing them keeps the light's total.

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform sampler2D e_HeightBuffer;
uniform vec2 e_GroundOrigin;    // world position of the ground grid's top-left
uniform vec2 e_GroundSize;      // world units
uniform vec2 e_ViewOrigin;      // world position of the layer buffers' top-left
uniform vec2 e_ViewSize;
uniform float e_Step;           // world units between samples up the column
uniform float e_Lod;            // emission mip about as wide as one sample
uniform int e_Samples;

out vec4 finalColor;

void main()
{
    vec2 ground = e_GroundOrigin + vec2(fragTexCoord.x, 1.0 - fragTexCoord.y) * e_GroundSize;
    vec3 sum = vec3(0.0);
    for (int i = 0; i < e_Samples; i++)
    {
        float lift = float(i) * e_Step;
        vec2 f = (ground - vec2(0.0, lift) - e_ViewOrigin) / e_ViewSize;
        if (any(lessThan(f, vec2(0.0))) || any(greaterThan(f, vec2(1.0)))) continue;
        vec2 st = vec2(f.x, 1.0 - f.y);
        float height = textureLod(e_HeightBuffer, st, 0.0).r;
        if (abs(height - lift) <= e_Step * 0.5) sum += textureLod(texture0, st, e_Lod).rgb;
    }
    finalColor = vec4(sum, 1.0);
}
