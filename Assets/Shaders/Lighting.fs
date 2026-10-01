#version 330

// The lighting pass of a lit layer (RenderCompositor::RenderLit). texture0 is the layer's
// albedo, drawn as usual; e_NormalBuffer and e_EmissionBuffer are the same records drawn
// through their materials' SurfaceNormal / SurfaceEmission. The light itself was gathered
// from the emission at low resolution by LightGather.fs: e_LightBuffer is what a flat
// surface receives, e_DirectionBuffer where it mostly comes from. The layer's buffers
// share texcoords; the light is a grid over the ground (e_GroundOrigin/Size), not the
// screen, so each pixel reads it where it stands: its world position, dropped by its
// height (e_HeightBuffer) onto its ground line. A wall's face is lit by what reaches its
// base, a unit's head by what reaches its feet.
//
//   color = albedo * (ambient + light, turned by the normal) + emission
//
// With e_PointLights the light comes from LightComponents instead. Each pixel is placed in
// the 3D world from its ground point and height (see WorldTo3D in RenderSystem.cpp), given
// the normal of the surface it's on (a floor faces up, a wall or a unit faces out from its
// ground line, turned by its normal map), and lit by every light that reaches it unless the
// light's cube shadow map (ShadowAtlas) says an occluder is in the way.

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform sampler2D e_NormalBuffer;
uniform sampler2D e_EmissionBuffer;
uniform sampler2D e_LightBuffer;
uniform sampler2D e_DirectionBuffer;
uniform sampler2D e_HeightBuffer;
uniform sampler2D e_OccluderMask;
uniform sampler2D e_OccluderField;
uniform sampler2D e_GroundEmission;

uniform vec2 e_ViewOrigin;      // world position of the layer buffers' top-left
uniform vec2 e_ViewSize;        // world units
uniform vec2 e_GroundOrigin;    // the light / occluder grid's
uniform vec2 e_GroundSize;
uniform vec2 e_GridSize;        // occluder grid texels
uniform int e_Shadows;
uniform int e_Debug;            // SceneLayer::lightDebug

const int kMaxLights = 8;       // ShadowAtlas::kMaxLights
uniform int e_PointLights;
uniform int e_LightCount;
uniform vec3 e_LightPos[kMaxLights];     // 3D world
uniform vec3 e_LightColor[kMaxLights];   // premultiplied by intensity
uniform float e_LightRadius[kMaxLights];
uniform float e_LightVision[kMaxLights]; // 1: also clears the fog where it can see
uniform float e_Fog;            // 0 off .. 1 the unseen is hidden
uniform vec3 e_FogColor;
uniform sampler2D e_ShadowAtlas;
uniform float e_ShadowRows;     // light rows in the atlas
uniform float e_ShadowTile;     // texels per face tile
uniform float e_ShadowBias;     // world units

uniform vec2 e_Resolution;
uniform vec3 e_Ambient;
uniform float e_Bands;
uniform float e_Outline;
uniform vec4 e_OutlineColor;

out vec4 finalColor;

vec3 NormalAt(vec2 st)
{
    // Partly covered pixels can blend to almost nothing; call those flat rather than NaN.
    vec3 n = texture(e_NormalBuffer, st).rgb * 2.0 - 1.0;
    return dot(n, n) > 1e-4 ? normalize(n) : vec3(0.0, 0.0, 1.0);
}

vec2 GroundSt(vec2 world)
{
    vec2 f = (world - e_GroundOrigin) / e_GroundSize;
    return vec2(f.x, 1.0 - f.y);
}

float OccluderHeight(vec2 world)
{
    return textureLod(e_OccluderMask, GroundSt(world), 0.0).r * 2040.0;
}

// Ink: where coverage or the normal changes sharply between neighbouring pixels.
vec4 Ink(vec4 lit, float alpha, vec3 n)
{
    vec2 texel = 1.0 / e_Resolution;
    float edge = 0.0;
    vec2 offsets[4] = vec2[](vec2(texel.x, 0.0), vec2(-texel.x, 0.0), vec2(0.0, texel.y), vec2(0.0, -texel.y));
    for (int i = 0; i < 4; i++)
    {
        vec2 st = fragTexCoord + offsets[i];
        edge = max(edge, abs(texture(texture0, st).a - alpha));
        edge = max(edge, (1.0 - dot(n, NormalAt(st))) * 2.0);
    }
    float ink = smoothstep(0.15, 0.6, edge) * e_Outline * e_OutlineColor.a;
    vec4 tint = colDiffuse * fragColor;
    return vec4(mix(lit.rgb, e_OutlineColor.rgb * tint.rgb, ink), max(alpha, ink) * tint.a);
}

// --- Point lights ------------------------------------------------------------------------

const float kIsoCos = 0.8660254;

// The face a direction from the light falls on, and where on it: the major axis picks the
// face, which looks along it with up +Y (+Z for the two Y faces). Must match ShadowAtlas.cpp.
float ShadowDistance(int light, vec3 v, vec2 offset)
{
    vec3 a = abs(v);
    int face;
    vec3 forward, up;
    if (a.x >= a.y && a.x >= a.z)
    {
        face = v.x > 0.0 ? 0 : 1; forward = vec3(sign(v.x), 0.0, 0.0); up = vec3(0.0, 1.0, 0.0);
    }
    else if (a.y >= a.z)
    {
        face = v.y > 0.0 ? 2 : 3; forward = vec3(0.0, sign(v.y), 0.0); up = vec3(0.0, 0.0, 1.0);
    }
    else
    {
        face = v.z > 0.0 ? 4 : 5; forward = vec3(0.0, 0.0, sign(v.z)); up = vec3(0.0, 1.0, 0.0);
    }
    vec3 right = cross(forward, up);
    vec2 uv = vec2(dot(v, right), dot(v, up)) / dot(v, forward) * 0.5 + 0.5;
    float inset = 1.0 / e_ShadowTile;
    uv = clamp(uv + offset / e_ShadowTile, vec2(inset), vec2(1.0 - inset));
    vec2 st = vec2((float(face) + uv.x) / 6.0, (float(light) + uv.y) / e_ShadowRows);
    return textureLod(e_ShadowAtlas, st, 0.0).r;
}

// 0 in shadow .. 1 lit, softened over a few texels.
float Shadow(int light, vec3 fromLight, float radius)
{
    float d = length(fromLight) / radius - 0.004;
    float lit = 0.0;
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++)
            lit += d < ShadowDistance(light, fromLight, vec2(x, y) * 1.25) ? 1.0 : 0.0;
    return lit / 9.0;
}

// The light reaching p, and (in seen) how clearly any vision light sees it: the same
// shadow test, faded out over the last fifth of its radius, and only for surfaces turned
// towards it (the back of a wall is out of sight from in front of it).
vec3 PointLight(vec3 p, vec3 surfaceNormal, vec3 n, out float seen)
{
    vec3 lifted = p + surfaceNormal * e_ShadowBias;
    vec3 total = vec3(0.0);
    seen = 0.0;
    for (int i = 0; i < kMaxLights; i++)
    {
        if (i >= e_LightCount) break;
        vec3 toLight = e_LightPos[i] - p;
        float d = length(toLight);
        float radius = max(e_LightRadius[i], 1.0);
        if (d >= radius) continue;
        bool eyes = e_LightVision[i] > 0.5;
        // A little wrap, so sprites drawn round don't go flat black at the terminator.
        float facing = clamp((dot(n, toLight / d) + 0.25) / 1.25, 0.0, 1.0);
        bool lights = dot(e_LightColor[i], e_LightColor[i]) > 0.0 && facing > 0.0;
        if (!lights && !(eyes && dot(surfaceNormal, toLight) > 0.0)) continue;
        float shadow = Shadow(i, lifted - e_LightPos[i], radius);
        if (eyes && dot(surfaceNormal, toLight) > 0.0)
            seen = max(seen, shadow * (1.0 - smoothstep(0.8, 1.0, d / radius)));
        if (!lights) continue;
        float falloff = 1.0 - d / radius;
        falloff *= falloff;
        total += e_LightColor[i] * falloff * facing * shadow;
    }
    return total;
}

vec4 DebugView(vec2 world, float height, vec2 groundSt, float alpha)
{
    if (e_Debug == 1)
    {
        // Occluders solid (brighter = taller), the field around them as contour rings.
        vec2 st = GroundSt(world);
        float occluder = OccluderHeight(world);
        if (occluder > 0.0) return vec4(vec3(0.4 + occluder / 200.0, 0.3, 0.2), 1.0);
        ivec2 texel = clamp(ivec2(st * e_GridSize), ivec2(0), ivec2(e_GridSize) - 1);
        vec4 seed = texelFetch(e_OccluderField, texel, 0);
        if (seed.b < 0.5) return vec4(0.0, 0.0, 0.0, 1.0);
        float d = distance(seed.xy, vec2(texel)) * e_GroundSize.x / e_GridSize.x;
        float ring = smoothstep(0.8, 1.0, fract(d / 16.0));
        return vec4(vec3(exp(-d / 96.0) * 0.5) + ring * vec3(0.1, 0.3, 0.5), 1.0);
    }
    if (e_Debug == 2) return vec4(vec3(height / 100.0) + vec3(0.0, 0.0, 0.15 * alpha), 1.0);
    if (e_Debug == 3) return vec4(texture(e_LightBuffer, groundSt).rgb, 1.0);
    return vec4(textureLod(e_GroundEmission, groundSt, 0.0).rgb, 1.0);
}

void main()
{
    vec4 albedo = texture(texture0, fragTexCoord);
    vec3 n = NormalAt(fragTexCoord);
    vec3 emission = textureLod(e_EmissionBuffer, fragTexCoord, 0.0).rgb;

    // Where this pixel stands. Anything standing (height > 0) shows its south face, so
    // step its ground point south out of the occluder it stands in: the wall is lit by
    // what reaches the room side of its base, not by what's behind it.
    vec2 world = e_ViewOrigin + vec2(fragTexCoord.x, 1.0 - fragTexCoord.y) * e_ViewSize;
    vec4 stand = textureLod(e_HeightBuffer, fragTexCoord, 0.0);
    float height = stand.r;
    vec2 ground = world + vec2(0.0, height);
    vec3 light;
    if (e_PointLights != 0)
    {
        // The surface: lying on the ground (normal up, the picture's up running away from
        // the camera), or standing on a ground line of slope stand.g (facing the camera).
        vec3 p = vec3(ground.x, height / kIsoCos, ground.y * 2.0);
        vec3 surfaceNormal = stand.b > 0.5 ? normalize(vec3(-2.0 * stand.g, 0.0, 1.0)) : vec3(0.0, 1.0, 0.0);
        // Normal maps are SpriteBaker's: view space (+x right, +y up the picture, +z toward
        // the camera), the camera looking down 30 degrees. Rotate them into the world; a
        // floor's map then comes out pointing up. A pixel with no map (exactly flat) takes
        // its surface's normal instead.
        vec3 n3 = surfaceNormal;
        if (n.z < 0.999)
        {
            const vec3 viewUp = vec3(0.0, kIsoCos, -0.5);
            const vec3 viewBack = vec3(0.0, 0.5, kIsoCos);
            n3 = normalize(n.x * vec3(1.0, 0.0, 0.0) + n.y * viewUp + n.z * viewBack);
        }
        if (e_Debug == 5)
        {
            finalColor = vec4(fract(p / 64.0) * 0.8 + 0.2 * surfaceNormal, 1.0);
            return;
        }
        float seen;
        light = PointLight(p, surfaceNormal, n3, seen);
        float fog = e_Fog * (1.0 - seen);
        if (e_Debug == 6)
        {
            finalColor = vec4(mix(e_Ambient + light, e_FogColor, fog) * albedo.a, 1.0);
            return;
        }
        if (e_Bands > 0.5) light = ceil(light * e_Bands - 0.05) / e_Bands;
        vec3 lit = mix(albedo.rgb * (e_Ambient + light) + emission, e_FogColor, fog);
        finalColor = vec4(lit, albedo.a) * colDiffuse * fragColor;
        if (e_Outline > 0.0) finalColor = Ink(finalColor, albedo.a, n);
        return;
    }
    if (height > 0.5 && e_Shadows != 0)
    {
        float texelWorld = e_GroundSize.x / e_GridSize.x;
        for (int i = 0; i < 16 && OccluderHeight(ground) > 0.0; i++) ground.y += texelWorld;
    }
    vec2 groundSt = GroundSt(ground);
    if (e_Debug != 0)
    {
        finalColor = DebugView(world, height, groundSt, albedo.a);
        return;
    }

    // The flat surface's light, re-aimed: a normal facing the light's average direction
    // gets more than a flat one, facing away gets less. Flat normals get it unchanged.
    light = texture(e_LightBuffer, groundSt).rgb;
    vec3 direction = texture(e_DirectionBuffer, groundSt).xyz;
    if (dot(direction, direction) > 1e-8)
    {
        vec3 toLight = normalize(direction);
        light *= clamp(max(dot(n, toLight), 0.0) / max(toLight.z, 0.2), 0.0, 3.0);
    }
    if (e_Bands > 0.5) light = ceil(light * e_Bands - 0.05) / e_Bands;
    vec3 color = albedo.rgb * (e_Ambient + light) + emission;

    finalColor = vec4(color, albedo.a) * colDiffuse * fragColor;
    if (e_Outline > 0.0) finalColor = Ink(finalColor, albedo.a, n);
}
