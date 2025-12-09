#include "../src/gpu_io.h"

[[vk::push_constant]] ConstantBuffer<RTPushConstants> pushConst;

[[vk::binding(0, 0)]] Sampler2D textures[100];
[[vk::binding(0, 1)]] RaytracingAccelerationStructure topLevelAS;
[[vk::binding(1, 1)]] RWTexture2D<float4> outImage;

static const float kMinT = 0.001f;
static const float kMaxT = 100.0f;

struct HitPayload
{
    float3      color;
    float       weight;
    float       distance;
    int         depth;
    uint32_t    rngState;
};

[shader("raygeneration")]
void rgMain()
{
    // Get the current pixel coordinates and image dimensions
    uint width, height;
    outImage.GetDimensions(width, height);
    uint pixelIdx = DispatchRaysIndex().x * DispatchRaysIndex().y * width;

    float2 launchID = (float2)DispatchRaysIndex().xy;     // Current pixel (x,y)
    float2 launchSize = (float2)DispatchRaysDimensions().xy; // Image size (width,height)

    Scene scene = pushConst.sceneRef[0];
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
    payload.color = float3(0, 0, 0);  // Start with black
    payload.weight = 1;               // Full weight for primary rays
    payload.distance = -1.0f;
    payload.depth = 0; // Start at depth 0
    payload.rngState = InitRNGState(DispatchRaysIndex().x, DispatchRaysIndex().y, pushConst.frame);

    // Cast the ray into the scene using the acceleration structure
    // Parameters: AS, flags, instance mask, sbt offset, sbt stride, miss offset, ray, payload
    TraceRay(topLevelAS, rayFlags, 0xff, 0, 0, 0, ray, payload);

    // Get the final color from the ray tracing result
    float3 color = payload.color;
    
    if (payload.depth == 0)
        return;

    // Write the result to the output image
    outImage[int2(launchID)] = float4(payload.color, 1.0);
}

// Closest hit shader - called when ray hits geometry
[shader("closesthit")]
void chMain(inout HitPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    Scene scene = pushConst.sceneRef[0];
    REF(Mesh) meshBufferRef = scene.meshBufferRef;
    REF(Instance) instanceBufferRef = scene.instanceBufferRef;
    Instance instance = instanceBufferRef[InstanceIndex()];
    Mesh mesh = meshBufferRef[instance.meshHandle];
    REF(Vertex) vertexBufferRef = mesh.vertexBufferRef;
    REF(uint32_t) indexBufferRef = mesh.indexBufferRef;
    REF(Material) materialBufferRef = scene.materialBufferRef;
    REF(Primitive) primitiveBufferRef = mesh.primitiveBufferRef;
    Material material = materialBufferRef[primitiveBufferRef[0].materialHandle];

    Sampler2D baseColorTexture  = textures[material.baseColorHandle];
    Sampler2D normalMapTexture  = textures[material.normalMapHandle];
    Sampler2D metalRoughTexture = textures[material.metalRoughHandle];
    Sampler2D emissiveTexture   = textures[material.emissiveHandle];

    // Triangle index inside the BLAS
    uint triIndex = PrimitiveIndex() * 3;

    // Fetch the three vertex indices
    uint i0 = indexBufferRef[triIndex + 0];
    uint i1 = indexBufferRef[triIndex + 1];
    uint i2 = indexBufferRef[triIndex + 2];

    // Fetch the three vertices
    Vertex v0 = vertexBufferRef[i0];
    Vertex v1 = vertexBufferRef[i1];
    Vertex v2 = vertexBufferRef[i2];

    // Interpolate attributes using barycentrics
    float w0 = 1.0 - attr.barycentrics.x - attr.barycentrics.y;
    float w1 = attr.barycentrics.x;
    float w2 = attr.barycentrics.y;

    float3 hitPos = v0.position.xyz * w0 + v1.position.xyz * w1 + v2.position.xyz * w2;
    float3 hitNormal = normalize(v0.normal.xyz * w0 + v1.normal.xyz * w1 + v2.normal.xyz * w2);
    float2 hitUV = v0.texCoord.xy * w0 + v1.texCoord.xy * w1 + v2.texCoord.xy * w2;

    // Use hitPos, hitNormal, hitUV for shading
    payload.color = float3(baseColorTexture.Sample(hitUV).rgb); // example: encode UVs as color
    //payload.color = float3(1.0, 0.0, 0.0); // example: encode UVs as color

    payload.depth = 1;
    payload.distance = RayTCurrent();
}

// Miss shader - called when ray doesn't hit any geometry
[shader("miss")]
void mMain(inout HitPayload payload)
{
    // Set background color when ray misses all geometry
    // In a full implementation, this could sample from:
    // - Skybox textures
    // - Procedural sky gradients
    // - Environment maps
    payload.color = float3(0.0, 0.0, 0.2); // Gray background
}