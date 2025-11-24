//glsl version 4.5
#version 450

//shader input
layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 inUV;
layout (location = 2) in mat3 inTBN;
//output write
layout (location = 0) out vec4 outFragColor;

layout(set = 0, binding = 0) uniform sampler2D baseColor;
layout(set = 0, binding = 1) uniform sampler2D normalMap;
layout(set = 0, binding = 2) uniform sampler2D metallicRoughness;
layout(set = 0, binding = 3) uniform sampler2D occlusion;
layout(set = 0, binding = 4) uniform sampler2D emissive;

layout( push_constant ) uniform constants
{	
	int renderMode;
	vec3 padding;
} PushConstants;

void main() 
{
	if (PushConstants.renderMode == 0)
	{
		//vec3 lightDir = normalize(vec3(0.525, 0.079, 0.842));
		//vec3 mapNormal = texture(normalMap, inUV).xyz * 2.0 - 1.0;
		//vec3 normal = inTBN * mapNormal;
		//float cosa = max(dot(normal, lightDir), 0.0);
		//vec3 albedo = texture(baseColor, inUV).rgb;
		//outFragColor = vec4(albedo * cosa, 1.0);
		outFragColor = vec4(0.5, 0.5, 0.5, 1.0);
	}
	else
	if (PushConstants.renderMode == 1)
	{
		outFragColor = vec4(inUV, 0.0, 1.0);
	}
	else
	if (PushConstants.renderMode == 2)
	{
		outFragColor = vec4((inTBN[0] + 1.0) * 0.5, 1.0);
	}
	else
	if (PushConstants.renderMode == 3)
	{
		outFragColor = vec4((inTBN[1] + 1.0) * 0.5, 1.0);
	}
	else
	if (PushConstants.renderMode == 4)
	{
		outFragColor = vec4((inTBN[2] + 1.0) * 0.5, 1.0);
	}
	else
	if (PushConstants.renderMode == 5)
	{
		vec3 mapNormal = texture(normalMap, inUV).xyz * 2.0 - 1.0;
		vec3 normal = inTBN * mapNormal;
		outFragColor = vec4((normal + 1.0) * 0.5, 1.0);
	}
	else
	if (PushConstants.renderMode == 6)
	{
		outFragColor = texture(baseColor, inUV);
	}
	else
	if (PushConstants.renderMode == 7)
	{
		outFragColor = texture(normalMap, inUV);
	}
	else
	if (PushConstants.renderMode == 8)
	{
		float metallic = texture(metallicRoughness, inUV).b;
		outFragColor = vec4(metallic, metallic, metallic, 1.0);
	}
	else
	if (PushConstants.renderMode == 9)
	{
		float roughness = texture(metallicRoughness, inUV).g;
		outFragColor = vec4(roughness, roughness, roughness, 1.0);
	}
	else
	if (PushConstants.renderMode == 10)
	{
		outFragColor = texture(occlusion, inUV);
	}
	else
	if (PushConstants.renderMode == 11)
	{
		outFragColor = texture(emissive, inUV);
	}
}