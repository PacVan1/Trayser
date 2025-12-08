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
#define PUSH_CONST(type) alignas(16) type // rule std140 for push constants
#define FLOAT4X4 float4x4
#define FLOAT3X3 float3x3
#define FLOAT2X2 float2x2
#define BEGIN_ENUM_DEF(type) enum type##_ {
#define DEF_ENUM_ENTRY(type, entry) type##_##entry,
#define DEF_ENUM_ENTRY_VAL(type, entry, value) type##_##entry = value,
#define END_ENUM_DEF(type) k##type##Count };
#else
#define REF(type) type*
#define PUSH_CONST(type) type
#define FLOAT4X4 column_major float4x4
#define FLOAT3X3 column_major float3x3
#define FLOAT2X2 column_major float2x2
#define BEGIN_ENUM_DEF(type) enum type {
#define DEF_ENUM_ENTRY(type, entry) entry,
#define DEF_ENUM_ENTRY_VAL(type, entry, value) entry = value,
#define END_ENUM_DEF(type) kCount, };
#endif

static constexpr float kGamma       = 2.2;
static constexpr float kInvGamma    = 1.0 / 2.2;
static constexpr float kPi          = 3.14159265359;

BEGIN_ENUM_DEF(RenderMode)
DEF_ENUM_ENTRY(RenderMode, FinalColor)
DEF_ENUM_ENTRY(RenderMode, TexCoord0)
DEF_ENUM_ENTRY(RenderMode, Tangent)
DEF_ENUM_ENTRY(RenderMode, Bitangent)
DEF_ENUM_ENTRY(RenderMode, GeometryNormal)
DEF_ENUM_ENTRY(RenderMode, ShadingNormal)
DEF_ENUM_ENTRY(RenderMode, BaseColor)
DEF_ENUM_ENTRY(RenderMode, NormalMap)
DEF_ENUM_ENTRY(RenderMode, Metallic)
DEF_ENUM_ENTRY(RenderMode, Roughness)
DEF_ENUM_ENTRY(RenderMode, AmbientOcclusion)
DEF_ENUM_ENTRY(RenderMode, Emission)
DEF_ENUM_ENTRY(RenderMode, WorldPos)
DEF_ENUM_ENTRY(RenderMode, ViewDir)
DEF_ENUM_ENTRY(RenderMode, Depth)
END_ENUM_DEF(RenderMode)

namespace gpu
{

struct Vertex
{
    float4  position;
    float4  normal;
    float4  tangent;
    float4  texCoord;
};

struct Mesh
{
    uint32_t        materialHandle;
    uint32_t        _pad[3];
	REF(Vertex)     vertexBufferRef;
	REF(uint32_t)   indexBufferRef;
};

struct Instance
{
    FLOAT4X4 transform;         // Transformation matrix
    FLOAT3X3 normalTransform;   // Transformation matrix
    uint32_t meshHandle;        // Index to mesh buffer
    uint32_t _pad[3];
};

struct Camera
{
    FLOAT4X4 viewProj;          // proj * view matrix
    FLOAT4X4 proj;              // Projection matrix
    FLOAT4X4 view;              // View matrix
    FLOAT4X4 invProj;           // Inverse projection matrix
    FLOAT4X4 invView;           // Inverse view matrix
};

struct Material
{
    uint32_t baseColorHandle;   // Base color index to textures
    uint32_t normalMapHandle;   // Normal map index to textures
    uint32_t metalRoughHandle;  // Metallic roughness index to textures
    uint32_t aoHandle;          // Ambient occlusion index to textures
    uint32_t emissiveHandle;    // Emissive index to textures

    float4   baseColorFactor;
    float4   metallicRoughnessAoFactor;
    float4   emissiveFactor;
};

struct Scene
{
    Camera          camera;
    REF(Mesh)       meshBufferRef;
    REF(Instance)   instanceBufferRef;
    REF(Material)   materialBufferRef;
};

struct PUSH_CONST(RTPushConstants)
{
    REF(Scene) sceneRef;
    int4 renderMode;
};

struct PUSH_CONST(RasterPushConstants)
{
    REF(Scene) sceneRef;
    int renderMode;
    int instanceIdx;
    int _pad[2];
};

} // namespace gpu

#ifdef __SLANG__
using namespace gpu;
float3 GetPosition(in Camera camera)
{
    return mul(camera.invView, float4(0, 0, 0, 1)).xyz;
}
#endif