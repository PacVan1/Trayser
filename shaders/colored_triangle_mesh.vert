//> all
#version 450
#extension GL_EXT_buffer_reference : require

layout (location = 0) out vec3 outColor;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec2 outUV;
layout (location = 3) out vec3 outTangent;
layout (location = 4) out vec3 outBitangent;

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
	//load vertex data from device adress
	Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];

	//output data
	mat4 mvp = PushConstants.viewProj * PushConstants.model;
	gl_Position = mvp * vec4(v.position, 1.0f);
	outColor = v.color.xyz;
	outUV.x = v.uv_x;
	outUV.y = v.uv_y;
	vec3 normal		= normalize(transpose(inverse(mat3(PushConstants.model))) * v.normal.xyz);
	vec3 tangent	= normalize(mat3(PushConstants.model) * v.tangent.xyz);
	tangent			= normalize(tangent - normal * dot(normal, tangent));
	vec3 bitangent	= cross(normal, tangent) * v.tangent.w;
	outNormal = normal;
	outTangent = tangent;
	outBitangent = bitangent;
}
//< all
