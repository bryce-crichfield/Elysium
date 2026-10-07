in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 uCardPos;
out vec4 finalColor;
void main()
{
    vec4 c = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    float seen;
    vec3 light = Light3D(uCardPos, vec3(0.0), seen);
    finalColor = vec4(Fogged(c.rgb * light, seen), c.a);
}
