#version 330 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;

uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

out vec3 fragPosition;
out vec3 fragNormal;
out vec2 fragTexCoord;
out float fragWaveHeight;

void main() {
    // Transform position
    vec4 worldPos = modelMatrix * vec4(position, 1.0);
    fragPosition = worldPos.xyz;

    // Transform normal
    mat3 normalMatrix = transpose(inverse(mat3(modelMatrix)));
    fragNormal = normalize(normalMatrix * normal);

    fragTexCoord = texCoord;
    fragWaveHeight = position.y;

    gl_Position = projectionMatrix * viewMatrix * worldPos;
}