#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;
in vec4 vertexBoneIds;
in vec4 vertexBoneWeights;
uniform mat4 mvp;
uniform mat4 matNormal;
uniform mat4 matModel;
uniform mat4 boneMatrices[128];
out vec2 fragTexCoord;
out vec3 fragNormal;
out vec3 fragWorld;
void main()
{
    mat4 skin = boneMatrices[int(vertexBoneIds.x)] * vertexBoneWeights.x
              + boneMatrices[int(vertexBoneIds.y)] * vertexBoneWeights.y
              + boneMatrices[int(vertexBoneIds.z)] * vertexBoneWeights.z
              + boneMatrices[int(vertexBoneIds.w)] * vertexBoneWeights.w;
    vec4 position = skin * vec4(vertexPosition, 1.0);
    vec3 normal = mat3(skin) * vertexNormal;
    fragTexCoord = vertexTexCoord;
    fragWorld = vec3(matModel * position);
    fragNormal = normalize(vec3(matNormal * vec4(normal, 0.0)));
    gl_Position = mvp * position;
}
