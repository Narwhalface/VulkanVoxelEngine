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

void main() {
    gl_Position = ubo.lightViewProj * ubo.model * vec4(inPosition, 1.0);
}