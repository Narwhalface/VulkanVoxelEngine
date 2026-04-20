#version 450

layout(std140, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 lightViewProj;
    vec4 lightDirection;
    vec4 lightColor;
    vec4 ambientColor;
    vec4 cameraPosition;
    vec4 shadowParams;
    vec4 debugParams;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragColor;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec4 fragShadowPosition;

void main() {
    vec4 worldPosition = ubo.model * vec4(inPosition, 1.0);
    mat3 normalMatrix = mat3(transpose(inverse(ubo.model)));

    gl_Position = ubo.proj * ubo.view * worldPosition;
    fragWorldPos = worldPosition.xyz;
    fragColor = inColor;
    fragNormal = normalize(normalMatrix * inNormal);
    fragShadowPosition = ubo.lightViewProj * worldPosition;
}