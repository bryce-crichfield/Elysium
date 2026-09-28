#version 330

// The lighting pass of a lit layer (RenderCompositor::RenderLit). texture0 is the layer's
// albedo, drawn as usual; e_NormalBuffer and e_EmissionBuffer are the same records drawn
// through their materials' SurfaceNormal / SurfaceEmission. The light itself was gathered
// from the emission at low resolution by LightGather.fs: e_LightBuffer is what a flat
// surface receives, e_DirectionBuffer where it mostly comes from. All share texcoords.
//
//   color = albedo * (ambient + light, turned by the normal) + emission

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform sampler2D e_NormalBuffer;
uniform sampler2D e_EmissionBuffer;
uniform sampler2D e_LightBuffer;
uniform sampler2D e_DirectionBuffer;

uniform vec2 e_Resolution;
uniform vec3 e_Ambient;
uniform float e_Bands;
uniform float e_Outline;
uniform vec4 e_OutlineColor;

out vec4 finalColor;

vec3 NormalAt(vec2 st)
{
    return normalize(texture(e_NormalBuffer, st).rgb * 2.0 - 1.0);
}

void main()
{
    vec4 albedo = texture(texture0, fragTexCoord);
    vec3 n = NormalAt(fragTexCoord);
    vec3 emission = textureLod(e_EmissionBuffer, fragTexCoord, 0.0).rgb;

    // The flat surface's light, re-aimed: a normal facing the light's average direction
    // gets more than a flat one, facing away gets less. Flat normals get it unchanged.
    vec3 light = texture(e_LightBuffer, fragTexCoord).rgb;
    vec3 direction = texture(e_DirectionBuffer, fragTexCoord).xyz;
    if (dot(direction, direction) > 1e-8)
    {
        vec3 toLight = normalize(direction);
        light *= clamp(max(dot(n, toLight), 0.0) / max(toLight.z, 0.2), 0.0, 3.0);
    }
    if (e_Bands > 0.5) light = ceil(light * e_Bands - 0.05) / e_Bands;
    vec3 color = albedo.rgb * (e_Ambient + light) + emission;

    // Ink: where coverage or the normal changes sharply between neighbouring pixels.
    if (e_Outline > 0.0)
    {
        vec2 texel = 1.0 / e_Resolution;
        float edge = 0.0;
        vec2 offsets[4] = vec2[](vec2(texel.x, 0.0), vec2(-texel.x, 0.0), vec2(0.0, texel.y), vec2(0.0, -texel.y));
        for (int i = 0; i < 4; i++)
        {
            vec2 st = fragTexCoord + offsets[i];
            edge = max(edge, abs(texture(texture0, st).a - albedo.a));
            edge = max(edge, (1.0 - dot(n, NormalAt(st))) * 2.0);
        }
        float ink = smoothstep(0.15, 0.6, edge) * e_Outline * e_OutlineColor.a;
        color = mix(color, e_OutlineColor.rgb, ink);
        albedo.a = max(albedo.a, ink);
    }

    finalColor = vec4(color, albedo.a) * colDiffuse * fragColor;
}
