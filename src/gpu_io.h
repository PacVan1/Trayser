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
#define REF(type) VkDeviceAddress
#else
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

} // namespace gpu