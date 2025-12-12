#include "gpu_io.h"

[[vk::push_constant]] ConstantBuffer<RTPushConstants> pushConsts;

[[vk::binding(0, 0)]] Sampler2D textures[128];
[[vk::binding(0, 1)]] RaytracingAccelerationStructure topLevelAS;
[[vk::binding(1, 1)]] RWTexture2D<float4> outImage;
[[vk::binding(2, 1)]] RWTexture2D<float4> outAccumulator;

static const float kMinT = 0.001f;
static const float kMaxT = 50.0f;

struct HitPayload
{
    //float       w0; // barycentric weight 0
    //float       w1; // barycentric weight 0
    //float       w2; // barycentric weight 0
    //uint32_t    geometryIndex;
    //uint32_t    instanceIndex;
    //uint32_t    primitiveIndex;

    float3 color;
    float weight;
    float distance;
    int depth;
    uint32_t rngState;
    uint32_t _pad;
};

[shader("raygeneration")]
void rgMain()
{
    float2 launchID = (float2)DispatchRaysIndex().xy;     // Current pixel (x,y)
    float2 launchSize = (float2)DispatchRaysDimensions().xy; // Image size (width,height)

    Scene scene = pushConsts.sceneRef[0];
    Camera camera = scene.camera;

    // Set ray tracing flags (0 = no special flags)
    const uint rayFlags = 0;
    
    // Convert pixel coordinates to normalized device coordinates (NDC)
    // Range: [-1,1] for both x and y
    const float2 clipCoords = launchID / launchSize * 2.0 - 1.0;

    // Transform from NDC to view space using inverse projection matrix
    const float4 viewCoords = mul(camera.invProj, float4(clipCoords, 1.0, 1.0));

    // Create the primary ray
    RayDesc ray;
    // Transform camera origin (0,0,0) from view space to world space
    ray.Origin = mul(camera.invView, float4(0.0, 0.0, 0.0, 1.0)).xyz;
    // Transform ray direction from view space to world space
    ray.Direction = mul(camera.invView, float4(normalize(viewCoords.xyz), 0.0)).xyz;
    ray.TMin = kMinT; // Minimum distance to avoid self-intersection
    ray.TMax = kMaxT; // Maximum distance (infinite for primary rays)

    // Initialize ray payload with default values
    HitPayload payload;
    payload.color = float3(1.0);  // Start with black
    payload.weight = 1;               // Full weight for primary rays
    payload.distance = -1.0f;
    payload.depth = 0; // Start at depth 0
    payload.rngState = InitRNGState(DispatchRaysIndex().x, DispatchRaysIndex().y, pushConsts.frame);

    // Cast the ray into the scene using the acceleration structure
    // Parameters: AS, flags, instance mask, sbt offset, sbt stride, miss offset, ray, payload
    TraceRay(topLevelAS, rayFlags, 0xff, 0, 0, 0, ray, payload);

    float fframe = float(pushConsts.frame);
    float scale = 1.0 / (fframe + 1.0);

    outAccumulator[int2(launchID)] += float4(payload.color, 1.0);
    outImage[int2(launchID)] = outAccumulator[int2(launchID)] * scale;
    //outImage[int2(launchID)] = float4(payload.color, 1.0);
}

float getBrdfProbability(MaterialProperties material, float3 V, float3 shadingNormal) {

    // Evaluate Fresnel term using the shading normal
    // Note: we use the shading normal instead of the microfacet normal (half-vector) for Fresnel term here. That's suboptimal for rough surfaces at grazing angles, but half-vector is yet unknown at this point
    float specularF0 = luminance(baseColorToSpecularF0(material.baseColor, material.metalness));
    float diffuseReflectance = luminance(baseColorToDiffuseReflectance(material.baseColor, material.metalness));
    float Fresnel = saturate(luminance(evalFresnel(specularF0, shadowedF90(specularF0), max(0.0f, dot(V, shadingNormal)))));

    // Approximate relative contribution of BRDFs using the Fresnel term
    float specular = Fresnel;
    float diffuse = diffuseReflectance * (1.0f - Fresnel); //< If diffuse term is weighted by Fresnel, apply it here as well

    // Return probability of selecting specular BRDF over diffuse BRDF
    float p = (specular / max(0.0001f, (specular + diffuse)));

    // Clamp probability to avoid undersampling of less prominent BRDF
    return clamp(p, 0.1f, 0.9f);
}

// Closest hit shader - called when ray hits geometry
[shader("closesthit")]
void chMain(inout HitPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    //payload.w0              = 1.0 - attr.barycentrics.x - attr.barycentrics.y;
    //payload.w1              = attr.barycentrics.x;
    //payload.w2              = attr.barycentrics.y;
    //payload.geometryIndex   = GeometryIndex();
    //payload.instanceIndex   = InstanceIndex();
    //payload.primitiveIndex  = PrimitiveIndex();

    if (payload.depth >= 2)
    {
        payload.color = float3(0.0);
        return;
    }

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

    Sampler2D baseColorTexture  = textures[material.baseColorHandle];
    Sampler2D normalMapTexture  = textures[material.normalMapHandle];
    Sampler2D metalRoughTexture = textures[material.metalRoughHandle];
    Sampler2D emissiveTexture   = textures[material.emissiveHandle];

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

    // Continue at fully transparent surfaces
    float3 position     = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    float2 uv           = v0.texCoord.xy * w0 + v1.texCoord.xy * w1 + v2.texCoord.xy * w2;
    float4 baseColor    = baseColorTexture.SampleLevel(uv, 0.0);

    if (baseColor.a == 0.0)
    {
        payload.distance += RayTCurrent();
        RayDesc ray;
        ray.Origin = position;
        ray.Direction = WorldRayDirection();
        ray.TMin = kMinT;
        ray.TMax = kMaxT;
        TraceRay(topLevelAS, RAY_FLAG_NONE, 0xff, 0, 0, 0, ray, payload);
        return; 
    }

    MaterialProperties matProps;
    matProps.baseColor          = baseColor.rgb;
    matProps.metalness          = metalRoughTexture.SampleLevel(uv, 0.0).b;
    matProps.roughness          = metalRoughTexture.SampleLevel(uv, 0.0).g;
    matProps.emissive           = emissiveTexture.SampleLevel(uv, 0.0).rgb;
    matProps.opacity            = 1.0;
    matProps.reflectance        = 0.5; // Default to 0.4 for minimal reflectence of 4%
    matProps.transmissivness    = 0.0;

    // Continue getting hit info
    float3 geometryNormal   = normalize(v0.normal.xyz * w0 + v1.normal.xyz * w1 + v2.normal.xyz * w2);
    float3 tangent          = normalize(v0.tangent.xyz * w0 + v1.tangent.xyz * w1 + v2.tangent.xyz * w2);

    float3x3 normalTransform = float3x3(instance.normalTransform);
    geometryNormal          = normalize(mul(normalTransform, geometryNormal));
    tangent                 = normalize(mul((float3x3)instance.transform, tangent.xyz));
    tangent                 = normalize(tangent - geometryNormal * dot(geometryNormal, tangent));
    float3 bitangent        = cross(geometryNormal, tangent) * v0.tangent.w;
    float3x3 tbn            = float3x3(tangent, bitangent, geometryNormal);
    float3 mapNormal        = normalize(normalMapTexture.SampleLevel(uv.xy, 0.0).xyz * 2.0 - 1.0);
    float3 shadingNormal    = normalize(mul(mapNormal, tbn));

    switch (pushConsts.renderMode)
    {
    case RenderMode.FinalColor:
    {
        //payload.color = evalCombinedBRDF(shadingNormal, normalize(float3(0.63921, 0.794, 0.15)), -WorldRayDirection(), matProps) * 2.0f + matProps.emissive;

        int brdfType;
        if (matProps.metalness == 1.0f && matProps.roughness == 0.0f) {
            // Fast path for mirrors
            brdfType = SPECULAR_TYPE;
        } else {

            // Decide whether to sample diffuse or specular BRDF (based on Fresnel term)
            float brdfProbability = getBrdfProbability(matProps, -WorldRayDirection(), shadingNormal);

            if (RandomFloat(payload.rngState) < brdfProbability) {
                brdfType = SPECULAR_TYPE;
                payload.color /= brdfProbability;
            } else {
                brdfType = DIFFUSE_TYPE;
                payload.color /= (1.0f - brdfProbability);
            }
        }

        // Run importance sampling of selected BRDF to generate reflecting ray direction
        float3 brdfWeight;
        float2 u = RandomFloat2(payload.rngState);
        float3 rayDirection;
        if (!evalIndirectCombinedBRDF(u, shadingNormal, geometryNormal, -WorldRayDirection(), matProps, brdfType, rayDirection, brdfWeight)) {
            break; // Ray was eaten by the surface :(
        }

        // Account for surface properties using the BRDF "weight"
        payload.color *= brdfWeight;

        payload.depth++;
        payload.distance = RayTCurrent();

        RayDesc ray;
        ray.Origin = position;
        ray.Direction = rayDirection;
        ray.TMin = kMinT;
        ray.TMax = kMaxT;
        TraceRay(topLevelAS, RAY_FLAG_NONE, 0xff, 0, 0, 0, ray, payload);
        return;
    }
    case RenderMode.TexCoord0:
        payload.color = float3(uv, 0.0);
        break;
    case RenderMode.GeometryNormal:
        payload.color = (float3(geometryNormal) + 1.0) * 0.5;
        break;
    case RenderMode.TransformedTangent:
        payload.color = (float3(tangent) + 1.0) * 0.5;
        break;
    case RenderMode.TransformedBitangent:
        payload.color = (float3(bitangent) + 1.0) * 0.5;
        break;
    case RenderMode.ShadingNormal:
        payload.color = (float3(shadingNormal) + 1.0) * 0.5;
        break;
    case RenderMode.BaseColor:
        payload.color = float3(LinearToSrgb(matProps.baseColor));
        break;
    case RenderMode.NormalMap:
        payload.color = float3(mapNormal);
        break;
    case RenderMode.Metallic:
        payload.color = float3(matProps.metalness);
        break;
    case RenderMode.Roughness:
        payload.color = float3(matProps.roughness);
        break;
    case RenderMode.AmbientOcclusion:
    {
        //payload.depth++;
        //payload.distance = RayTCurrent();
        //
        //RayDesc ray;
        //ray.Origin = hit.position;
        //ray.Direction = RandomCosineOnHemisphere(hit.normal, payload.rngState);
        //ray.TMin = kMinT;
        //ray.TMax = kMaxT;
        //
        //TraceRay(topLevelAS, RAY_FLAG_NONE, 0xff, 0, 0, 0, ray, payload);
        return;
    }
    case RenderMode.Emission:
        payload.color = float3(LinearToSrgb(matProps.emissive));
        break;
    case RenderMode.WorldPos:
        payload.color = float3(position);
        break;
    case RenderMode.Depth:
        payload.color = float3(RayTCurrent() / kMaxT);
        break;
    case RenderMode.GeometryIndex:
    {
        uint32_t hash = WangHash(GeometryIndex());
        payload.color = float3(RandomFloat(hash), RandomFloat(hash), RandomFloat(hash));
        break;  
    }
    case RenderMode.PrimitiveIndex:
    {
        uint32_t hash = WangHash(PrimitiveIndex());
        payload.color = float3(RandomFloat(hash), RandomFloat(hash), RandomFloat(hash));
        break;
    }
    default:
        payload.color = float3(1.0, 0.0, 1.0); // Default color: magenta
        break;
    }

    payload.depth++;
    payload.distance = RayTCurrent();
}

[shader("miss")]
void mMain(inout HitPayload payload)
{
    Scene scene = pushConsts.sceneRef[0];

    float2 uv = float2(atan2(WorldRayDirection().z, WorldRayDirection().x), asin(-WorldRayDirection().y));
    uv *= kInvTan;
    uv += 0.5;

    payload.color *= textures[scene.skydomeHandle].SampleLevel(uv, 0.0).rgb;
}