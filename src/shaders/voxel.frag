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

layout(binding = 1) uniform sampler2D shadowMap;

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec4 fragShadowPosition;

layout(location = 0) out vec4 outColor;

bool shadowCoordIsOutOfBounds(vec3 shadowCoord, vec2 uv) {
	return shadowCoord.z <= 0.0 || shadowCoord.z >= 1.0 || uv.x <= 0.0 || uv.x >= 1.0 || uv.y <= 0.0 || uv.y >= 1.0;
}

float sampleShadow(vec3 worldPos, vec3 normal, vec3 lightDir) {
	float topFaceFactor = smoothstep(0.35, 0.85, normal.y);
	float sideFaceFactor = 1.0 - topFaceFactor;
	float receiverNormalOffset = 0.008 * topFaceFactor;
	float receiverLightOffset = 0.004 * topFaceFactor;
	vec3 offsetWorldPos = worldPos + normal * receiverNormalOffset + lightDir * receiverLightOffset;
	vec4 offsetShadowPosition = ubo.lightViewProj * vec4(offsetWorldPos, 1.0);
	vec3 shadowCoord = offsetShadowPosition.xyz / max(offsetShadowPosition.w, 1e-5);
	vec2 uv = shadowCoord.xy * 0.5 + 0.5;

	if (shadowCoordIsOutOfBounds(shadowCoord, uv)) {
		return 0.0;
	}

	float bias = max(ubo.shadowParams.y * (1.0 - max(dot(normal, lightDir), 0.0)), ubo.shadowParams.z);
	bias += sideFaceFactor * 0.0032;
	vec2 texelSize = vec2(1.0 / max(ubo.shadowParams.x, 1.0));
	float occlusion = 0.0;
	float totalWeight = 0.0;
	const float kernelRadius = 2.0;

	for (int y = -2; y <= 2; ++y) {
		for (int x = -2; x <= 2; ++x) {
			vec2 offset = vec2(x, y);
			float distanceWeight = 1.0 - clamp(length(offset) / (kernelRadius + 0.5), 0.0, 1.0);
			float closestDepth = texture(shadowMap, uv + offset * texelSize).r;
			occlusion += (shadowCoord.z - bias > closestDepth ? 1.0 : 0.0) * distanceWeight;
			totalWeight += distanceWeight;
		}
	}

	return totalWeight > 0.0 ? occlusion / totalWeight : 0.0;
}

void main() {
	vec3 normal = normalize(fragNormal);
	vec3 lightDir = normalize(-ubo.lightDirection.xyz);
	vec3 viewDir = normalize(ubo.cameraPosition.xyz - fragWorldPos);
	vec3 halfVector = normalize(lightDir + viewDir);
	vec3 shadowCoord = fragShadowPosition.xyz / max(fragShadowPosition.w, 1e-5);
	vec2 shadowUv = shadowCoord.xy * 0.5 + 0.5;
	bool shadowOutOfBounds = shadowCoordIsOutOfBounds(shadowCoord, shadowUv);
	float shadow = sampleShadow(fragWorldPos, normal, lightDir);
	float directLightVisibility = 1.0 - shadow * ubo.shadowParams.w;
	float ambientVisibility = 1.0 - shadow * 0.45;

	if (ubo.debugParams.x > 0.5) {
		if (shadowOutOfBounds) {
			outColor = vec4(1.0, 0.0, 1.0, 1.0);
			return;
		}

		outColor = vec4(vec3(directLightVisibility), 1.0);
		return;
	}

	float diffuse = max(dot(normal, lightDir), 0.0);
	float specular = pow(max(dot(normal, halfVector), 0.0), 48.0) * smoothstep(0.0, 0.2, diffuse) * directLightVisibility;
	float hemi = normal.y * 0.5 + 0.5;

	vec3 skyBounce = vec3(0.42, 0.62, 0.78) * hemi;
	vec3 groundBounce = vec3(0.16, 0.13, 0.10) * (1.0 - hemi);
	vec3 ambient = (ubo.ambientColor.xyz + skyBounce * 0.35 + groundBounce * 0.18) * ambientVisibility;

	vec3 litColor = fragColor * (ambient + diffuse * directLightVisibility * ubo.lightColor.xyz);
	litColor += specular * ubo.lightColor.xyz * 0.20;

	float distanceToCamera = length(ubo.cameraPosition.xyz - fragWorldPos);
	float fogFactor = smoothstep(180.0, 520.0, distanceToCamera);
	vec3 fogColor = vec3(0.53, 0.81, 0.92);
	vec3 finalColor = mix(litColor, fogColor, fogFactor);

	outColor = vec4(finalColor, 1.0);
}