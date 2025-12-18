#include "gpu_io.h"

#define ANTI_ALIASING
#define BRDF_SAMPLING
#define LIGHT_SAMPLING
#define MULTIPLE_IMPORTANCE_SAMPLING
#define SAMPLING_TECHNIQUE MULTIPLE_IMPORTANCE_SAMPLING

[[vk::push_constant]] ConstantBuffer<RTPushConstants> pushConsts;

[[vk::binding(0, 0)]] Sampler2D textures[128];
[[vk::binding(0, 1)]] RaytracingAccelerationStructure tlas;
[[vk::binding(1, 1)]] RWTexture2D<float4> outImage;
[[vk::binding(2, 1)]] RWTexture2D<float4> outAccumulator;

static const float kMinT        = 0.01f;
static const float kMaxT        = 50.0f;

static const uint32_t kMaxBounces = 3;

struct HitPayload
{
    float3      radiance;
    uint32_t    rngState;
    float3      throughput;
    int         bouncesLeft; // Exit path tracing loop
    float3      hitPosition;
    float3      nextDirection;
};

float3 SampleSkydome(in float3 rayDirection)
{
    Scene scene = pushConsts.sceneRef[0];
    float2 uv = float2(atan2(rayDirection.z, rayDirection.x), asin(-rayDirection.y));
    uv *= kInvTan;
    uv += 0.5;
    return textures[scene.skydomeHandle].SampleLevel(uv, 0.0).rgb;
}

float3 GetCameraPosition(in float4x4 invView)
{
    return invView[3].xyz;
}

void GetPrimaryRayDesc(in float4x4 invView, in float4x4 invProj, in float2 pixelCoord, in float2 dimension, out RayDesc ray)
{
    float2 clipCoords = pixelCoord / dimension * 2.0 - 1.0;
    float4 viewCoords = mul(invProj, float4(clipCoords, 1.0, 1.0));

    ray.Origin      = mul(invView, float4(0.0, 0.0, 0.0, 1.0)).xyz;
    ray.Direction   = mul(invView, float4(normalize(viewCoords.xyz), 0.0)).xyz;
    ray.TMin        = kMinT;
    ray.TMax        = kMaxT;
}

// Clever offset_ray function from Ray Tracing Gems chapter 6
// Offsets the ray origin from current position p, along normal n (which must be geometric normal)
// so that no self-intersection can occur.
float3 OffsetRay(const float3 p, const float3 n)
{
    static const float origin = 1.0f / 32.0f;
    static const float float_scale = 1.0f / 65536.0f;
    static const float int_scale = 256.0f;

    int3 of_i = int3(int_scale * n.x, int_scale * n.y, int_scale * n.z);

    float3 p_i = float3(
		asfloat(asint(p.x) + ((p.x < 0) ? -of_i.x : of_i.x)),
                        asfloat(asint(p.y) + ((p.y < 0) ? -of_i.y : of_i.y)),
                        asfloat(asint(p.z) + ((p.z < 0) ? -of_i.z : of_i.z)));

    return float3(abs(p.x) < origin ? p.x + float_scale * n.x : p_i.x,
                  abs(p.y) < origin ? p.y + float_scale * n.y : p_i.y,
                  abs(p.z) < origin ? p.z + float_scale * n.z : p_i.z);
}

[shader("raygeneration")]
void rgMain()
{
    uint2 pixelCoord    = DispatchRaysIndex().xy; // Current pixel (x,y)

    Scene scene     = pushConsts.sceneRef[0];
    Camera camera   = scene.camera;

    HitPayload payload; 
    payload.radiance    = 0.0;
    payload.throughput  = 1.0;
    payload.rngState    = InitRNGState(pixelCoord.x, pixelCoord.y, pushConsts.frame);

    // Initialize primary ray
    RayDesc rayDesc;
    float2 fpixelCoord  = pixelCoord;
    #ifdef ANTI_ALIASING
    fpixelCoord         = fpixelCoord + RandomFloat2(payload.rngState);
    #endif
    float2 fdimension   = (float2)DispatchRaysDimensions().xy; // Image size (width,height)
    GetPrimaryRayDesc(camera.invView, camera.invProj, fpixelCoord, fdimension, rayDesc);

    for (payload.bouncesLeft = kMaxBounces; payload.bouncesLeft > 0; payload.bouncesLeft--)
    {
        TraceRay(tlas, RAY_FLAG_NONE, 0xff, 0, 0, 0, rayDesc, payload);
        rayDesc.Origin = payload.hitPosition;
        rayDesc.Direction = payload.nextDirection;
    }

    float fframe = float(pushConsts.frame);
    float scale = 1.0 / (fframe + 1.0);

    outAccumulator[pixelCoord] += float4(payload.radiance, 1.0);
    outImage[pixelCoord] = outAccumulator[pixelCoord] * scale;
}

// PDF of light sampling from a spherical light source
// NdotL is the cosine of the angle between the shading normal and the direction to the light
// r is the radius of the light sphere
// d is the distance from the shading point to the light source

float computeLightPdf(float3 lightDir, float3 shadingNormal, float lightRadius, float distanceToLight) {
    // Compute cosine of the angle between shading normal and light direction
    float NdotL = max(0.0f, dot(shadingNormal, lightDir));

    // PDF based on area of the light and the cosine term
    return NdotL / (4.0f * kPi * lightRadius * lightRadius * distanceToLight * distanceToLight);
}

// Closest hit shader - called when ray hits geometry
[shader("closesthit")]
void chMain(inout HitPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    // Collect resources
    Scene scene = pushConsts.sceneRef[0];
    REF(Mesh) meshBufferRef = scene.meshBufferRef;
    REF(Instance) instanceBufferRef = scene.instanceBufferRef;
    Instance instance = instanceBufferRef[InstanceIndex()];
    Mesh mesh = meshBufferRef[instance.meshHandle];
    REF(Vertex) vertexBufferRef = mesh.vertexBufferRef;
    REF(uint32_t) indexBufferRef = mesh.indexBufferRef;
    REF(Material) materialBufferRef = scene.materialBufferRef;
    REF(Primitive) primitiveBufferRef = mesh.primitiveBufferRef;
    Primitive primitive = primitiveBufferRef[GeometryIndex()];
    Material material = materialBufferRef[primitive.materialHandle];
    REF(PointLight) pointLights = scene.lights.pointLightBufferRef;
    REF(SphereLight) sphereLights = scene.lights.sphereLightBufferRef;

    // Find triangle, vertices and barycentrics
    uint triangleIdx = primitive.baseIndex + PrimitiveIndex() * 3;

    uint i0 = indexBufferRef[triangleIdx + 0];
    uint i1 = indexBufferRef[triangleIdx + 1];
    uint i2 = indexBufferRef[triangleIdx + 2];

    Vertex v0 = vertexBufferRef[primitive.baseVertex + i0];
    Vertex v1 = vertexBufferRef[primitive.baseVertex + i1];
    Vertex v2 = vertexBufferRef[primitive.baseVertex + i2];

    float w0 = 1.0 - attr.barycentrics.x - attr.barycentrics.y;
    float w1 = attr.barycentrics.x;
    float w2 = attr.barycentrics.y;

    Sampler2D baseColorTexture = textures[material.baseColorHandle];
    Sampler2D normalMapTexture = textures[material.normalMapHandle];
    Sampler2D metalRoughTexture = textures[material.metalRoughHandle];
    Sampler2D emissiveTexture = textures[material.emissiveHandle];

    // Continue at fully transparent surfaces
    float2 uv = v0.texCoord.xy * w0 + v1.texCoord.xy * w1 + v2.texCoord.xy * w2;

    MaterialProperties matProps;
    matProps.baseColor = baseColorTexture.SampleLevel(uv, 0.0).rgb * material.baseColorFactor.rgb;
    matProps.metalness = metalRoughTexture.SampleLevel(uv, 0.0).b * material.metallicRoughnessAoFactor.r;
    matProps.roughness = metalRoughTexture.SampleLevel(uv, 0.0).g * material.metallicRoughnessAoFactor.g;
    matProps.emissive = emissiveTexture.SampleLevel(uv, 0.0).rgb * material.emissiveFactor.rgb;
    matProps.opacity = 1.0;
    matProps.reflectance = 0.5; // Default to 0.5 for minimal reflectence of 4%
    matProps.transmissivness = 0.0;

    // Continue getting hit info
    float3 position = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    float3 geometryNormal = normalize(v0.normal.xyz * w0 + v1.normal.xyz * w1 + v2.normal.xyz * w2);
    float3x3 normalTransform = float3x3(instance.normalTransform);
    float3 geometryTangent = normalize(v0.tangent.xyz * w0 + v1.tangent.xyz * w1 + v2.tangent.xyz * w2);
    geometryNormal = normalize(mul(normalTransform, geometryNormal));
    float3 tangent = normalize(mul((float3x3)instance.transform, geometryTangent.xyz));
    tangent = normalize(tangent - geometryNormal * dot(geometryNormal, tangent));
    float3 bitangent = cross(geometryNormal, tangent) * v0.tangent.w;
    float3x3 tbn = float3x3(tangent, bitangent, geometryNormal);
    float3 mapNormal = normalize(normalMapTexture.SampleLevel(uv.xy, 0.0).xyz * 2.0 - 1.0);
    float3 shadingNormal = normalize(mul(mapNormal, tbn));

    //payload.radiance += payload.throughput * matProps.emissive;

    switch (pushConsts.renderMode)
    {
    case RenderMode.FinalColor:
    {
        // Point lights
        for (int i = 0; i < kSphereLightCount; i++)
        {
            SphereLight sphereLight = sphereLights[i];
            float3 random = RandomPointWithinSphere(payload.rngState, sphereLight);
            float3 L = random - position;
            float dist = length(L);
            float att = 1.0 / (dist * dist);
            L = normalize(L);
            float3 light = evalCombinedBRDF(shadingNormal, L, -WorldRayDirection(), matProps) * att * sphereLight.color * sphereLight.intensity;
            payload.radiance += payload.throughput * light;
        }

        // TEMP for MIS test
        //    int brdfType;
        //    if (matProps.metalness == 1.0f && matProps.roughness == 0.0f) {
        //        // Fast path for mirrors
        //        brdfType = SPECULAR_TYPE;
        //    } else {
        //    
        //        // Decide whether to sample diffuse or specular BRDF (based on Fresnel term)
        //        float brdfProbability = GetBrdfProbability(matProps, -WorldRayDirection(), shadingNormal);
        //    
        //        if (RandomFloat(payload.rngState) < brdfProbability) {
        //            brdfType = SPECULAR_TYPE;
        //            payload.throughput /= brdfProbability;
        //        } else {
        //            brdfType = DIFFUSE_TYPE;
        //            payload.throughput /= (1.0f - brdfProbability);
        //        }
        //    }
        //    
        //    // Run importance sampling of selected BRDF to generate reflecting ray direction
        //    float3 brdfWeight;
        //    float2 u = RandomFloat2(payload.rngState);
        //    float3 rayDirection;
        //    if (!evalIndirectCombinedBRDF(u, shadingNormal, geometryNormal, -WorldRayDirection(), matProps, brdfType, rayDirection, brdfWeight)) {
        //        payload.bouncesLeft = 0;
        //        return; // Ray was eaten by the surface :(
        //    }
        // Account for surface properties using the BRDF "weight"
        //    payload.throughput *= brdfWeight;
        //    payload.hitPosition = OffsetRay(position, geometryNormal);
        //    payload.nextDirection = rayDirection;
        payload.bouncesLeft = 0;
        return;
    }
    case RenderMode.TexCoord0:
        payload.radiance = float3(uv, 0.0);
        payload.bouncesLeft = 0;
        break;
    case RenderMode.GeometryNormal:
        payload.radiance = (float3(geometryNormal) + 1.0) * 0.5;
        payload.bouncesLeft = 0;
        break;
    case RenderMode.GeometryTangent:
        payload.radiance = (float3(geometryTangent) + 1.0) * 0.5;
        payload.bouncesLeft = 0;
        break;
    case RenderMode.Handedness:
        payload.radiance = float3(v0.tangent.w);
        payload.bouncesLeft = 0;
        break;
    case RenderMode.TransformedTangent:
        payload.radiance = (float3(tangent) + 1.0) * 0.5;
        payload.bouncesLeft = 0;
        break;
    case RenderMode.TransformedBitangent:
        payload.radiance = (float3(bitangent) + 1.0) * 0.5;
        payload.bouncesLeft = 0;
        break;
    case RenderMode.ShadingNormal:
        payload.radiance = (float3(shadingNormal) + 1.0) * 0.5;
        payload.bouncesLeft = 0;
        break;
    case RenderMode.BaseColor:
        payload.radiance = float3(LinearToSrgb(matProps.baseColor));
        payload.bouncesLeft = 0;
        break;
    case RenderMode.NormalMap:
        payload.radiance = (float3(mapNormal) + 1.0) * 0.5;
        payload.bouncesLeft = 0;
        break;
    case RenderMode.Metallic:
        payload.radiance = float3(matProps.metalness);
        payload.bouncesLeft = 0;
        break;
    case RenderMode.Roughness:
        payload.radiance = float3(matProps.roughness);
        payload.bouncesLeft = 0;
        break;
    case RenderMode.Emission:
        payload.radiance = float3(LinearToSrgb(matProps.emissive));
        payload.bouncesLeft = 0;
        break;
    case RenderMode.WorldPos:
        payload.radiance = float3(position);
        payload.bouncesLeft = 0;
        break;
    case RenderMode.Depth:
        payload.radiance = float3(RayTCurrent() / kMaxT);
        payload.bouncesLeft = 0;
        break;
    case RenderMode.GeometryIndex:
    {
        uint32_t hash = WangHash(GeometryIndex());
        payload.radiance = float3(RandomFloat(hash), RandomFloat(hash), RandomFloat(hash));
        payload.bouncesLeft = 0;
        break;
    }
    case RenderMode.PrimitiveIndex:
    {
        uint32_t hash = WangHash(PrimitiveIndex());
        payload.radiance = float3(RandomFloat(hash), RandomFloat(hash), RandomFloat(hash));
        payload.bouncesLeft = 0;
        break;
    }
    default:
        payload.radiance = float3(1.0, 0.0, 1.0); // Default color: magenta
        payload.bouncesLeft = 0;
        break;
    }
}

[shader("miss")]
void mMain(inout HitPayload payload)
{
    payload.radiance += payload.throughput * SampleSkydome(WorldRayDirection());
    payload.bouncesLeft = 0;
}