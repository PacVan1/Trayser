#include "../src/gpu_io.h"

[[vk::push_constant]] ConstantBuffer<RTPushConstants> pushConsts;

[[vk::binding(0, 0)]] Sampler2D textures[128];
[[vk::binding(0, 1)]] RaytracingAccelerationStructure topLevelAS;
[[vk::binding(1, 1)]] RWTexture2D<float4> outImage;

static const float kMinT = 0.001f;
static const float kMaxT = 50.0f;

struct HitPayload
{
    float3      color;
    float       weight;
    float       distance;
    int         depth;
    uint32_t    rngState;
    uint32_t    _pad;
};

struct HitInfo
{
    float3  albedo;
    float   metallic;
    float3  normal;
    float   roughness;
    float3  position;
    float   _pad1;
    float3  tangent;
    float   _pad2;
    float3  bitangent;
    float   _pad3;
    float3  emission;
    float   _pad4;
    float2  uv;
}

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

    // Get the final color from the ray tracing result
    float3 color = payload.color;
    
    //if (payload.depth == 0)
    //    return;

    // Write the result to the output image
    outImage[int2(launchID)] = float4(payload.color, 1.0);
}

// Closest hit shader - called when ray hits geometry
[shader("closesthit")]
void chMain(inout HitPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
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

    // Get hit info
    HitInfo hit;
    hit.position    = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    hit.normal      = normalize(v0.normal.xyz * w0 + v1.normal.xyz * w1 + v2.normal.xyz * w2);
    hit.tangent     = v0.tangent.xyz * w0 + v1.tangent.xyz * w1 + v2.tangent.xyz * w2;
    hit.uv          = v0.texCoord.xy * w0 + v1.texCoord.xy * w1 + v2.texCoord.xy * w2;
    hit.albedo      = baseColorTexture.Sample(hit.uv).rgb;
    hit.emission    = emissiveTexture.Sample(hit.uv).rgb;
    hit.metallic    = metalRoughTexture.Sample(hit.uv).b;
    hit.roughness   = metalRoughTexture.Sample(hit.uv).g;

    hit.normal = normalize(mul(instance.normalTransform, hit.normal));
    hit.tangent = normalize(mul((float3x3)instance.transform, hit.tangent.xyz));
    hit.tangent = normalize(hit.tangent - hit.normal * dot(hit.normal, hit.tangent));
    hit.bitangent = cross(hit.normal, hit.tangent) * v0.tangent.w;
    float3x3 tbn = float3x3(hit.tangent, hit.bitangent, hit.normal);
    float3 mapNormal = normalize(normalMapTexture.Sample(hit.uv.xy).xyz * 2.0 - 1.0);
    hit.normal = normalize(mul(mapNormal, tbn));

    switch (pushConsts.renderMode)
    {
    case RenderMode.FinalColor:
        payload.color *= hit.albedo;
        payload.depth++;
        payload.distance = RayTCurrent();

        RayDesc ray;
        ray.Origin = hit.position;
        ray.Direction = RandomCosineOnHemisphere(hit.normal, payload.rngState);
        ray.TMin = kMinT;
        ray.TMax = kMaxT;

        TraceRay(topLevelAS, RAY_FLAG_NONE, 0xff, 0, 0, 0, ray, payload);
        return;
    case RenderMode.TexCoord0:
        payload.color = float3(hit.uv, 0.0);
        break;
    case RenderMode.Tangent:
        payload.color = (float3(hit.tangent) + 1.0) * 0.5;
        break;
    case RenderMode.Bitangent:
        payload.color = (float3(hit.bitangent) + 1.0) * 0.5;
        break;
    case RenderMode.GeometryNormal:
        payload.color = (float3(hit.normal) + 1.0) * 0.5;
        break;
    case RenderMode.ShadingNormal:
        payload.color = (float3(hit.normal) + 1.0) * 0.5;
        break;
    case RenderMode.BaseColor:
        payload.color = float3(LinearToSrgb(hit.albedo));
        break;
    case RenderMode.NormalMap:
        payload.color = float3(mapNormal);
        break;
    case RenderMode.Metallic:
        payload.color = float3(hit.metallic);
        break;
    case RenderMode.Roughness:
        payload.color = float3(hit.roughness);
        break;
    case RenderMode.AmbientOcclusion:
    {
        payload.depth++;
        payload.distance = RayTCurrent();

        RayDesc ray;
        ray.Origin = hit.position;
        ray.Direction = RandomCosineOnHemisphere(hit.normal, payload.rngState);
        ray.TMin = kMinT;
        ray.TMax = kMaxT;

        TraceRay(topLevelAS, RAY_FLAG_NONE, 0xff, 0, 0, 0, ray, payload);
        return;
    }
    case RenderMode.Emission:
        payload.color = float3(LinearToSrgb(hit.emission));
        break;
    case RenderMode.WorldPos:
        payload.color = float3(hit.position);
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

    payload.color *= textures[scene.skydomeHandle].Sample(uv).rgb;
}