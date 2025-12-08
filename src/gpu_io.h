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
#define FLOAT4X4 float4x4
#define FLOAT3X3 float3x3
#define FLOAT2X2 float2x2
#else
#define REF(type) type*
#define FLOAT4X4 column_major float4x4
#define FLOAT3X3 column_major float3x3
#define FLOAT2X2 column_major float2x2
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
    FLOAT4X4 transform;         // Transformation matrix
    uint32_t meshHandle;        // Index to mesh buffer
};

struct Camera
{
    FLOAT4X4 viewProj;          // proj * view matrix
    FLOAT4X4 proj;              // Projection matrix
    FLOAT4X4 view;              // View matrix
    FLOAT4X4 invProj;           // Inverse projection matrix
    FLOAT4X4 invView;           // Inverse view matrix
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
    REF(Scene) sceneRef2;
    int4 renderMode;
};

struct RasterPushConstants
{
    FLOAT4X4 transform;
    REF(Scene) sceneRef;
    REF(Scene) sceneRef2;
    int4 renderMode;
};

} // namespace gpu

#ifdef __SLANG__
using namespace gpu;
float3 GetPosition(in Camera camera)
{
    return float3(camera.invView[3].xyz);
}
#endif