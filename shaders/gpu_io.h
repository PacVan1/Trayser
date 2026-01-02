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
#include "../external/BRDF/brdf.h"
#include "mis.h"
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

#define SPHERE_LIGHT_COUNT  5
#define POINT_LIGHT_COUNT   10
#define DIR_LIGHT_COUNT     10

#define USING_MIS
//#define USING_BRDF_SAMPLING
//#define USING_LIGHT_SAMPLING

static constexpr uint32_t kTextureCount     = 128;
static constexpr uint32_t kSphereLightCount = 5;
static constexpr uint32_t kPointLightCount  = 10;
static constexpr uint32_t kDirLightCount    = 10;
static constexpr float kGamma               = 2.2;
static constexpr float kInvGamma            = 1.0 / 2.2;
static constexpr float kPi                  = 3.14159265359;
static constexpr float k2Pi                 = 2.0 * kPi;
static constexpr float kInvPi               = 1.0 / kPi;
static constexpr float2 kInvTan             = float2(0.1591, 0.3183);
static constexpr float kEpsilon             = 1e-3;

BEGIN_ENUM_DEF(RenderMode)
DEF_ENUM_ENTRY(RenderMode, FinalColor)
DEF_ENUM_ENTRY(RenderMode, TexCoord0)
DEF_ENUM_ENTRY(RenderMode, GeometryTangent)
DEF_ENUM_ENTRY(RenderMode, GeometryNormal)
DEF_ENUM_ENTRY(RenderMode, Handedness)
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

struct SphereLight
{
    float3  position;
    float   radius;     // scale
    float3  color;
    float   radius2;   // sqr(scale)
    float   intensity;
    float   area;
    float   _pad[2];
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
    PointLight          pointLights[POINT_LIGHT_COUNT];
    DirectionalLight    dirLights[DIR_LIGHT_COUNT];
    SphereLight         sphereLights[SPHERE_LIGHT_COUNT];
};

struct MaterialParams
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
    Camera              camera;
    Lights              lights;
    REF(Mesh)           meshBufferRef;
    REF(Instance)       instanceBufferRef;
    REF(MaterialParams) materialBufferRef;
    uint32_t            skydomeHandle;
    uint32_t            _pad[3];
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
struct HitInfo
{
    float3              P;
    float3              V;
    float3              sN;
    float3              gN;
    MaterialProperties  mat;
}

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
float3 SphericalToCartesian(in float rho, in float phi, in float theta) 
{
    // https://www.shadertoy.com/view/4sSXWt

    float sinTheta = sin(theta);
    return float3(sinTheta * cos(phi), sinTheta * sin(phi), cos(theta)) * rho;
}
float3 RandomDirection(inout uint32_t seed)
{
    // https://www.shadertoy.com/view/4sSXWt

    float theta = acos(1.0 - 2.0 * RandomFloat(seed));
    float phi = TWO_PI * RandomFloat(seed);
    return SphericalToCartesian(1.0, phi, theta);
}
float3 RandomPointWithinSphere(inout uint32_t seed, in SphereLight sphere)
{
    float3 dir = RandomDirection(seed);
    return sphere.position + dir * sphere.radius;
}
struct LightSampleInfo
{
    float3 outDir;
    float3 color;
    float pdf;
};
void UniformDirectionWithinCone(inout uint32_t rngState, in float3 dir, in float dist, in float radius, out LightSampleInfo outInfo)
{
    float sinThetaMax   = radius / dist;
    float cosThetaMax   = sqrt(1.0 - clamp(sinThetaMax * sinThetaMax, 0.0, 1.0));
    float cosTheta      = lerp(cosThetaMax, 1.0, RandomFloat(rngState));
    float sinTheta2     = 1.0 - cosTheta * cosTheta;
    float sinTheta      = sqrt(sinTheta2);
    float3 u            = normalize(cross(dir.yzx, dir));
    float3 v            = cross(dir, u);
    float phi           = k2Pi * RandomFloat(rngState);
    outInfo.pdf         = 1.0 / (k2Pi * (1.0 - cosThetaMax));
    outInfo.outDir      = (u * cos(phi) + v * sin(phi)) * sinTheta + dir * cosTheta;
}
void SampleLightSource(inout uint32_t rngState, in HitInfo hit, in SphereLight sphere, out LightSampleInfo outInfo)
{
    float3 dir  = sphere.position - hit.P;	// Direction to light center
    float dist2 = dot(dir, dir);		    // Squared distance to light center
    float dist  = sqrt(dist2);		        // Distance to light center
    dir = normalize(dir);

    UniformDirectionWithinCone(rngState, dir, dist, sphere.radius, outInfo);
    outInfo.color = sphere.color * sphere.intensity;
}
float3 SampleLight(inout uint32_t rngState, in HitInfo hit, in SphereLight sphere)
{
    float3 contribution = float3(0.0);

    LightSampleInfo info;
    SampleLightSource(rngState, hit, sphere, info);

    float dotNWo = dot(info.outDir, hit.sN);

    if ((dotNWo > 0.0) && (info.pdf > kEpsilon))
    {
        float3 brdf = evalCombinedBRDF(hit.sN, info.outDir, hit.V, hit.mat);
        //if (dot(brdf, brdf) > 0.0)
        //{
            contribution = (info.color * brdf * dotNWo) / info.pdf;
        //}
    }

    return contribution;
}
float3 UniformSampleCone(in float2 u, in float cosThetaMax, in float3 xbasis, in float3 ybasis, in float3 zbasis)
{
    float cosTheta = (1.0 - u.x) + u.x * cosThetaMax;
    float sinTheta = sqrt(1. - cosTheta * cosTheta);
    float phi = u.y * TWO_PI;
    float3 samplev = PolarToCartesian(sinTheta, cosTheta, sin(phi), cos(phi));
    return samplev.x * xbasis + samplev.y * ybasis + samplev.z * zbasis;
}
float3 PolarToCartesian(float sinTheta,
    float cosTheta,
    float sinPhi,
    float cosPhi)
{
    return float3(sinTheta * cosPhi,
        sinTheta * sinPhi,
        cosTheta);
}
void CalcBinormals(float3 normal,
    out float3 tangent,
    out float3 binormal)
{
    if (abs(normal.x) > abs(normal.y))
    {
        tangent = normalize(float3(-normal.z, 0., normal.x));
    }
    else
    {
        tangent = normalize(float3(0., normal.z, -normal.y));
    }

    binormal = cross(normal, tangent);
}
float DistSquared(float3 v1, float3 v2)
{
    return (v1.x - v2.x) * (v1.x - v2.x) +
        (v1.y - v2.y) * (v1.y - v2.y) +
        (v1.z - v2.z) * (v1.z - v2.z);
}
float4 IntersectSphere(in float3 rayO, in float3 rayD, in SphereLight sphere)
{
    if (DistSquared(rayO, sphere.position) < sphere.radius2)
    {
        return float4(-1.0, float3(0.0));
    }

    float3 sphro = rayO - sphere.position;
    float a = dot(rayD, rayD);
    float b = dot(sphro, rayD);
    float c = dot(sphro, sphro) - sphere.radius2;
    float sign = lerp(-1.0, 1.0, step(0.0, a));
    float t = (-b + sign * sqrt(b * b - a * c)) / a;

    float3 n = normalize(rayO + t * rayD - sphere.position);
    return float4(step(0.0, t), n);

}
float3 SampleLight2(inout uint32_t seed, in HitInfo hit, in SphereLight light, out float pdf)
{
    float2 u = RandomFloat2(seed);

    float3 tangent = float3(0.0), binormal = float3(0.0);
    float3 ldir = normalize(light.position - hit.P);
    CalcBinormals(ldir, tangent, binormal);

    float sinThetaMax2 = light.radius2 / DistSquared(light.position, hit.P);
    float cosThetaMax = sqrt(max(0.0, 1.0 - sinThetaMax2));
    float3 lightSample = UniformSampleCone(u, cosThetaMax, tangent, binormal, ldir);

    pdf = -1.0;
    if (dot(lightSample, hit.sN) > 0.0)
    {
        pdf = 1.0 / (k2Pi * (1.0 - cosThetaMax));
    }

    return lightSample;
}
float3 IntegrateLight(inout uint32_t seed, in HitInfo hit, in SphereLight light)
{        
    float lpdf = -1.0;
    float3 lightSample = SampleLight2(seed, hit, light, lpdf);

    if (lpdf > 0.0)
    {
        float4 r = IntersectSphere(hit.P, lightSample, light);
        if (r.x > .0)
        {
            float3 le = light.color * light.intensity / (r.x * r.x);
            return evalCombinedBRDF(hit.sN, lightSample, hit.V, hit.mat) *
                le / lpdf;

            //brdf::microfacet::Material material;
			//material.baseColor = hit.mat.baseColor;
			//material.metalness = hit.mat.metalness;
			//material.roughness = hit.mat.roughness;
            //material.emissive = hit.mat.emissive;
            //
            //return brdf::microfacet::EvalBrdf(hit.V, lightSample, material, hit.sN) * le / lpdf;
        }
    }
    return float3(0.0);
}
float PowerHeuristic(float nf,
    float fPdf,
    float ng,
    float gPdf)
{
    float f = nf * fPdf;
    float g = ng * gPdf;
    return (f * f) / (f * f + g * g);
}
float LightPdf(SphereLight light,
    HitInfo hit)
{
    float sinThetaMax2 = light.radius2 / DistSquared(light.position, hit.P);
    float cosThetaMax = sqrt(max(0.0, 1.0 - sinThetaMax2));
    return 1.0 / (k2Pi * (1.0 - cosThetaMax));
}
float3 IntegrateLightMIS(inout uint32_t seed, in HitInfo hit, in SphereLight light)
{
    float3 contribution = 0.0;

    // sample light
    float lpdf = -1.0;
    float3 lightSample = SampleLight2(seed, hit, light, lpdf);

    if (lpdf > 0.0)
    {
        float4 r = IntersectSphere(hit.P, lightSample, light);
        if (r.x > .0)
        {
            BrdfData brdfData = prepareBRDFData(hit.sN, lightSample, hit.V, hit.mat);
            float specPdf = specularPdf(brdfData.alpha, brdfData.alphaSquared, brdfData.NdotH, brdfData.NdotV, brdfData.LdotH);
            float diffPdf = diffusePdf(brdfData.NdotL);
            float brdfPdf = diffPdf * 0.5 + specPdf * 0.5;
            float misWeight = PowerHeuristic(1.0, lpdf, 1.0, brdfPdf);

            float3 le = light.color * light.intensity / (r.x * r.x);
            contribution += evalCombinedBRDF(hit.sN, lightSample, hit.V, hit.mat) *
                le * (misWeight / lpdf);
        }
    }

    int brdfType;
    if (hit.mat.metalness == 1.0f && hit.mat.roughness == 0.0f)
    {
        // Fast path for mirrors
        brdfType = SPECULAR_TYPE;
    }
    else
    {

        // Decide whether to sample diffuse or specular BRDF (based on Fresnel term)
        float brdfProbability = GetBrdfProbability(hit.mat, -WorldRayDirection(), hit.sN);

        if (RandomFloat(seed) < brdfProbability)
        {
            brdfType = SPECULAR_TYPE;
            //payload.throughput /= brdfProbability;
        }
        else
        {
            brdfType = DIFFUSE_TYPE;
            //payload.throughput /= (1.0f - brdfProbability);
        }
    }

    // sample brdf       
    float3 brdfW = 0.0;
    float3 brdfSample = 0.0;
    bool isShaded = evalIndirectCombinedBRDF(RandomFloat2(seed), hit.sN, hit.gN, hit.V, hit.mat, brdfType, brdfSample, brdfW);
    if (isShaded)
    {
        BrdfData brdfData = prepareBRDFData(hit.sN, brdfSample, hit.V, hit.mat);
        float specPdf = specularPdf(brdfData.alpha, brdfData.alphaSquared, brdfData.NdotH, brdfData.NdotV, brdfData.LdotH);
        float diffPdf = diffusePdf(brdfData.NdotL);
        float brdfPdf = brdfType == DIFFUSE_TYPE ? diffPdf : specPdf;

        float4 r = IntersectSphere(hit.P, brdfSample, light);
        if (r.x > 0.0)
        {
            float lpdf = LightPdf(light, hit);
            float misWeight = PowerHeuristic(1.0, brdfPdf, 1.0, lpdf);

            float3 le = light.color * light.intensity / (r.x * r.x);
            contribution += evalCombinedBRDF(hit.sN, brdfSample, hit.V, hit.mat) *
                le * (misWeight / brdfPdf);

        }
    }

    return contribution;
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