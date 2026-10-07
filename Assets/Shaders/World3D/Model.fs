in vec2 fragTexCoord;
in vec3 fragNormal;
in vec3 fragWorld;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float uAlpha;
uniform vec3 uEmissive;
out vec4 finalColor;
void main()
{
    vec4 albedo = texture(texture0, fragTexCoord) * colDiffuse;
    if (albedo.a < 0.5) discard;
    // Faded (uAlpha < 1): screen-door dither, so no sorting against what's behind.
    const float bayer[16] = float[16](0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0, 3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0);
    ivec2 cell = ivec2(gl_FragCoord.xy) % 4;
    if (uAlpha < (bayer[cell.y * 4 + cell.x] + 0.5) / 16.0) discard;
    vec3 n = normalize(fragNormal);
    float seen;
    vec3 light = Light3D(fragWorld, n, seen);
    // Rim: brightest where the surface turns edge-on to the camera.
    vec3 view = uPerspective > 0.5 ? normalize(uEye - fragWorld) : normalize(uTowardCamera);
    float rim = pow(1.0 - max(dot(n, view), 0.0), 3.0);
    light += uRim * rim * (uAmbient + uSunColor + vec3(0.25));
    finalColor = vec4(Fogged(albedo.rgb * light, seen) + uEmissive, albedo.a);
}
