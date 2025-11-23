//glsl version 4.5
#version 450

//shader input
layout (location = 0) in vec3 inColor;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inTangent;
layout (location = 4) in vec3 inBitangent;
//output write
layout (location = 0) out vec4 outFragColor;

layout(set = 0, binding = 0) uniform sampler2D displayTexture;

layout( push_constant ) uniform constants
{	
	int renderMode;
	vec3 padding;
} PushConstants;

void main() 
{
	if (PushConstants.renderMode == 0)
	{
		outFragColor = vec4(inUV, 0.0, 1.0);
	}
	else
	if (PushConstants.renderMode == 1)
	{
		outFragColor = vec4((inTangent + 1.0) * 0.5, 1.0);
	}
	else
	if (PushConstants.renderMode == 2)
	{
		outFragColor = vec4((inBitangent + 1.0) * 0.5, 1.0);
	}
	else
	if (PushConstants.renderMode == 3)
	{
		outFragColor = vec4((inNormal + 1.0) * 0.5, 1.0);
	}
	else
	if (PushConstants.renderMode == 5)
	{
		outFragColor = texture(displayTexture, inUV);
	}
}