#pragma once

#ifndef __SLANG__
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
using float4x4	= glm::mat4;
using float3x3	= glm::mat3;
using float2x2	= glm::mat2;
using float4	= glm::vec4;
using float3	= glm::vec3;
using float2	= glm::vec2;
using uint4     = glm::uvec4;
using uint3     = glm::uvec3;
using uint2     = glm::uvec2;
using int4      = glm::ivec4;
using int3      = glm::ivec3;
using int2      = glm::ivec2;
#define REF(type) VkDeviceAddress
#else
#pragma pack_matrix(column_major)
#define REF(type) type*
#endif

namespace gpu
{

struct Vertex
{
    float3  position;
    float   uvX;
    float3  normal;
    float   uvY;
    float4  color;
    float3  tangent;
    float   handedness;
};

struct Mesh
{
	REF(Vertex)     vertexBufferRef;
	REF(uint32_t)   indexBufferRef;
};

struct Instance
{
    float4x4 transform;         // Transformation matrix
    uint32_t meshHandle;        // Index to mesh buffer
};

struct Camera
{
    float4x4 proj;              // Projection matrix
    float4x4 view;              // View matrix
    float4x4 invProj;           // Inverse projection matrix
    float4x4 invView;           // Inverse view matrix
    float3 camPos;
};

struct Material
{
    uint32_t baseColorHandle;   // Base color index to textures
    uint32_t normalMapHandle;   // Normal map index to textures
    uint32_t metalRoughHandle;  // Metallic roughness index to textures
    uint32_t aoHandle;          // Ambient occlusion index to textures
    uint32_t emissiveHandle;    // Emissive index to textures
};

struct Scene
{
    Camera camera;

    REF(Material)   materialBufferRef;
    REF(Mesh)       meshBufferRef;
    REF(Instance)   instanceBufferRef;
};

struct RTPushConstants
{
    REF(Scene) sceneRef;
    int4 renderMode;
};

} // namespace gpu