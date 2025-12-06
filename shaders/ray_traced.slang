[[vk::push_constant]]
cbuffer PushConstants
{
    [[vk::offset(0)]]   column_major float4x4 viewInvMatrix;
    [[vk::offset(64)]]  column_major float4x4 projInvMatrix;
    [[vk::offset(128)]] float4 camPos;
    [[vk::offset(144)]] int4 renderMode;
};

// Texture array for material textures
//[[vk::binding(BindingPoints::eTextures, 0)]]    Sampler2D textures[];
// Top-level acceleration structure containing the scene geometry
[[vk::binding(0, 0)]] RaytracingAccelerationStructure topLevelAS;
// Output image where the final rendered result will be stored
[[vk::binding(1, 0)]] RWTexture2D<float4> outImage;
// clang-format on

static const float kMinT = 0.001f;
static const float kMaxT = 100.0f;

// Ray payload structure - carries data through the ray tracing pipeline
struct HitPayload
{
    float3 color;   // Accumulated color along the ray path
    float  weight;  // Weight/importance of this ray (for importance sampling)
    float  distance;
    int    depth;   // Current recursion depth (for limiting bounces)
};

// Ray generation shader - entry point for each pixel
[shader("raygeneration")]
void rgMain()
{
    // Get the current pixel coordinates and image dimensions
    float2 launchID = (float2)DispatchRaysIndex().xy;     // Current pixel (x,y)
    float2 launchSize = (float2)DispatchRaysDimensions().xy; // Image size (width,height)

    // Set ray tracing flags (0 = no special flags)
    const uint rayFlags = 0;
    
    // Convert pixel coordinates to normalized device coordinates (NDC)
    // Range: [-1,1] for both x and y
    const float2 clipCoords = launchID / launchSize * 2.0 - 1.0;

    // Transform from NDC to view space using inverse projection matrix
    const float4 viewCoords = mul(projInvMatrix, float4(clipCoords, 1.0, 1.0));

    // Create the primary ray
    RayDesc ray;
    // Transform camera origin (0,0,0) from view space to world space
    ray.Origin = mul(viewInvMatrix, float4(0.0, 0.0, 0.0, 1.0)).xyz;
    // Transform ray direction from view space to world space
    ray.Direction = mul(viewInvMatrix, float4(normalize(viewCoords.xyz), 0.0)).xyz;
    ray.TMin = kMinT; // Minimum distance to avoid self-intersection
    ray.TMax = kMaxT; // Maximum distance (infinite for primary rays)

    // Initialize ray payload with default values
    HitPayload payload;
    payload.color = float3(0, 0, 0);  // Start with black
    payload.weight = 1;               // Full weight for primary rays
    payload.distance = -1.0f;
    payload.depth = 0;                // Start at depth 0

    // Cast the ray into the scene using the acceleration structure
    // Parameters: AS, flags, instance mask, sbt offset, sbt stride, miss offset, ray, payload
    TraceRay(topLevelAS, rayFlags, 0xff, 0, 0, 0, ray, payload);

    // Get the final color from the ray tracing result
    float3 color = float3(payload.distance / kMaxT);
    
    if (payload.depth == 0)
        return;

    // Write the result to the output image
    outImage[int2(launchID)] = float4(color, 1.0);
}

// Closest hit shader - called when ray hits geometry
[shader("closesthit")]
void chMain(inout HitPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    // For now, just set a simple solid red color for all hits
    // In a full implementation, this would:
    // 1. Sample material properties (albedo, normal, etc.)
    // 2. Calculate lighting (direct + indirect)
    // 3. Potentially cast secondary rays (reflection, refraction)
    payload.color = float3(0.2, 0, 0); // Red color
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