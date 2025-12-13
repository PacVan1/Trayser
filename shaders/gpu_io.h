#pragma once

#ifndef __SLANG__
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
using float4x4	= glm::mat4;
using float3x3	= glm::mat3;
using float2x2	= glm::mat2;
using float3x4	= glm::mat3x4;
using float4x3	= glm::mat4x3;
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
#define FLOAT4X3 float4x3
#define FLOAT3X4 float3x4
#define BEGIN_ENUM_DEF(type) enum type##_ {
#define DEF_ENUM_ENTRY(type, entry) type##_##entry,
#define DEF_ENUM_ENTRY_VAL(type, entry, value) type##_##entry = value,
#define END_ENUM_DEF(type) k##type##Count };
#else
#include "brdf.h"
#define REF(type) type*
#define PUSH_CONST(type) type
#define FLOAT4X4 column_major float4x4
#define FLOAT3X3 column_major float3x3
#define FLOAT2X2 column_major float2x2
#define FLOAT4X3 column_major float4x3
#define FLOAT3X4 column_major float3x4
#define BEGIN_ENUM_DEF(type) enum type {
#define DEF_ENUM_ENTRY(type, entry) entry,
#define DEF_ENUM_ENTRY_VAL(type, entry, value) entry = value,
#define END_ENUM_DEF(type) kCount, };
#endif

static constexpr uint32_t kTextureCount     = 128;
static constexpr uint32_t kPointLightCount  = 10;
static constexpr uint32_t kDirLightCount    = 10;
static constexpr float kGamma               = 2.2;
static constexpr float kInvGamma            = 1.0 / 2.2;
static constexpr float kPi                  = 3.14159265359;
static constexpr float k2Pi                 = 2.0 * kPi;
static constexpr float kInvPi               = 1.0 / kPi;
static constexpr float2 kInvTan             = float2(0.1591, 0.3183);

BEGIN_ENUM_DEF(RenderMode)
DEF_ENUM_ENTRY(RenderMode, FinalColor)
DEF_ENUM_ENTRY(RenderMode, TexCoord0)
DEF_ENUM_ENTRY(RenderMode, GeometryTangent)
DEF_ENUM_ENTRY(RenderMode, GeometryBitangent)
DEF_ENUM_ENTRY(RenderMode, GeometryNormal)
DEF_ENUM_ENTRY(RenderMode, TransformedTangent)
DEF_ENUM_ENTRY(RenderMode, TransformedBitangent)
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
DEF_ENUM_ENTRY(RenderMode, GeometryIndex)
DEF_ENUM_ENTRY(RenderMode, PrimitiveIndex)
DEF_ENUM_ENTRY(RenderMode, Lambertian)
DEF_ENUM_ENTRY(RenderMode, Phong)
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

struct Primitive
{
    uint32_t materialHandle;
    uint32_t baseVertex;
    uint32_t vertexCount;
    uint32_t baseIndex;
    uint32_t indexCount;
};

struct Mesh
{
    REF(Primitive)  primitiveBufferRef;
	REF(Vertex)     vertexBufferRef;
	REF(uint32_t)   indexBufferRef;
};

struct Instance
{
    FLOAT4X4 transform;         // Transformation matrix
    FLOAT4X4 normalTransform;   // 4x3 because of alignment
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

struct PointLight
{
    float3 position;
    float  intensity;
    float3 color;
    float  _pad;
};

struct DirectionalLight
{
    float3 directional;
    float  intensity;
    float3 color;
    float  _pad;
};

struct Lights
{
    REF(PointLight)         pointLightBufferRef;
    REF(DirectionalLight)   dirLightBufferRef;
};

struct Material
{
    float4   baseColorFactor;
    float4   metallicRoughnessAoFactor;
    float4   emissiveFactor;
    uint32_t baseColorHandle;   // Base color index to textures
    uint32_t normalMapHandle;   // Normal map index to textures
    uint32_t metalRoughHandle;  // Metallic roughness index to textures
    uint32_t aoHandle;          // Ambient occlusion index to textures
    uint32_t emissiveHandle;    // Emissive index to textures
    uint32_t _pad[3];
};

struct Scene
{
    Camera          camera;
    Lights          lights;
    REF(Mesh)       meshBufferRef;
    REF(Instance)   instanceBufferRef;
    REF(Material)   materialBufferRef;
    uint32_t        skydomeHandle;
    uint32_t        _pad[3];
};

struct PUSH_CONST(RTPushConstants)
{
    REF(Scene) sceneRef;
    int renderMode;
    uint32_t frame;
    uint32_t _pad[2];
};

struct PUSH_CONST(RasterPushConstants)
{
    REF(Scene) sceneRef;
    int renderMode;
    int instanceIdx;
    int primitiveIdx;
    int _pad;
};

} // namespace gpu

#ifdef __SLANG__
using namespace gpu;
float3 GetPosition(in Camera camera)
{
    return mul(camera.invView, float4(0, 0, 0, 1)).xyz;
}
// WangHash: calculates a high-quality seed based on an arbitrary non-zero
// integer. Use this to create your own seed based on e.g. thread index.
uint32_t WangHash(in uint32_t seed)
{
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return seed;
}
// random number generator - Marsaglia's xor32
// This is a high-quality RNG that uses a single 32-bit seed. More info:
// https://www.researchgate.net/publication/5142825_Xorshift_RNGs
uint32_t RandomUInt(inout uint32_t seed)
{
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
}
// Calculate a random unsigned int and cast it to a float in the range [0..1)
float RandomFloat(inout uint32_t seed)
{ 
    return RandomUInt(seed) * 2.3283064365387e-10f; 
}
float2 RandomFloat2(inout uint32_t seed)
{
    return float2(RandomFloat(seed), RandomFloat(seed));
}
float3 RandomFloat3(inout uint32_t seed)
{
    return float3(RandomFloat(seed), RandomFloat(seed), RandomFloat(seed));
}
float3 RandomUnitFloat3(inout uint32_t seed)
{
    return normalize(float3(RandomFloat(seed), RandomFloat(seed), RandomFloat(seed)));
}
float3 RandomOnHemisphere(in float3 normal, inout uint32_t seed)
{
    float3 random = RandomUnitFloat3(seed);
    if (dot(random, normal) > 0.0f)
        return random;
    return -random;
}
float3 RandomCosineOnHemisphere(in float3 normal, inout uint32_t seed)
{
    float u1 = RandomFloat(seed);
    float u2 = RandomFloat(seed);

    float r = sqrt(u1);
    float theta = 2.0f * kPi * u2;

    float x = r * cos(theta);
    float y = r * sin(theta);
    float z = sqrt(1.0f - u1);

    float3 dir = float3(x, y, z);

    // Transform into world space using the normal
    float3 up = fabs(normal.z) < 0.999f ? float3(0, 0, 1) : float3(1, 0, 0);
    float3 tangent = normalize(cross(up, normal));
    float3 bitangent = cross(normal, tangent);

    return dir.x * tangent + dir.y * bitangent + dir.z * normal;
}
uint32_t InitRNGState(uint32_t x, uint32_t y, uint32_t frame)
{
    return WangHash((x * 0x1f123bb5u) ^ (y * 0x9e3779b9u) ^ (frame * 0x85ebca6bu)) + 1;
}
float SrgbToLinear(float color)
{
    return pow(color, kGamma);
}
float2 SrgbToLinear(float2 color)
{
    return pow(color, kGamma);
}
float3 SrgbToLinear(float3 color)
{
    return pow(color, kGamma);
}
float4 SrgbToLinear(float4 color)
{
    return pow(color, kGamma);
}
float LinearToSrgb(float color)
{
    return pow(color, kInvGamma);
}
float2 LinearToSrgb(float2 color)
{
    return pow(color, kInvGamma);
}
float3 LinearToSrgb(float3 color)
{
    return pow(color, kInvGamma);
}
float4 LinearToSrgb(float4 color)
{
    return pow(color, kInvGamma);
}
#endif