//> all
#version 450
#extension GL_EXT_buffer_reference : require

layout(location = 0) in vec3 inPosition;
layout(location = 1) in float inUVX;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in float inUVY;
layout(location = 4) in vec4 inColor;
layout(location = 5) in vec4 inTangent;

layout (location = 0) out vec3 outColor;
layout (location = 1) out vec2 outUV;
layout (location = 2) out mat3 outTBN;

struct Vertex {

	vec3 position;
	float uv_x;
	vec3 normal;
	float uv_y;
	vec4 color;
	vec4 tangent;
}; 

layout(buffer_reference, std430) readonly buffer VertexBuffer{ 
	Vertex vertices[];
};

//push constants block
layout( push_constant ) uniform constants
{	
	mat4 viewProj;
	mat4 model;
	VertexBuffer vertexBuffer;
} PushConstants;

void main() 
{	
	//output data
	mat4 mvp = PushConstants.viewProj * PushConstants.model;
	gl_Position = mvp * vec4(inPosition, 1.0f);
	outColor = inColor.xyz;
	outUV.x = inUVX;
	outUV.y = inUVY;
	vec3 normal		= normalize(transpose(inverse(mat3(PushConstants.model))) * inNormal);
	vec3 tangent	= normalize(mat3(PushConstants.model) * inTangent.xyz);
	tangent			= normalize(tangent - normal * dot(normal, tangent));
	vec3 bitangent	= cross(normal, tangent) * inTangent.w;
	outTBN = mat3(tangent, bitangent, normal);
}
//< all
