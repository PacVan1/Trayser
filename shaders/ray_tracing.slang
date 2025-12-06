[[vk::push_constant]]
cbuffer PushConstants
{
    [[vk::offset(0)]]   column_major float4x4 viewInvMatrix;
    [[vk::offset(64)]]   column_major float4x4 projInvMatrix;
};

// Texture array for material textures
//[[vk::binding(BindingPoints::eTextures, 0)]]    Sampler2D textures[];
// Top-level acceleration structure containing the scene geometry
[[vk::binding(0, 0)]] RaytracingAccelerationStructure topLevelAS;
// Output image where the final rendered result will be stored
[[vk::binding(1, 0)]] RWTexture2D<float4> outImage;
// clang-format on

static const float kMinT = 0.001f;
static const float kMaxT = 5.0f;

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
    float2 launchID = (float2)DispatchRaysIndex().xy;
    float2 launchSize = (float2)DispatchRaysDimensions().xy;

    // Normalized device coordinates [-1,1]
    float2 ndc = (launchID / launchSize) * 2.0 - 1.0;

    // Hardcoded camera setup
    float3 camPos = float3(3.0, 0.0, 0.0); // camera 3 units off center
    float3 target = float3(0.0, 0.0, 0.0); // look at origin
    float3 up = float3(0.0, 1.0, 0.0);     // world up
    float fovY = radians(60.0);            // vertical field of view

    // Build camera basis
    float3 forward = normalize(target - camPos);
    float3 right = normalize(cross(up, forward));
    float3 camUp = cross(forward, right);

    // Aspect ratio
    float aspect = launchSize.x / launchSize.y;

    // Scale NDC by FOV
    float px = ndc.x * aspect * tan(fovY * 0.5);
    float py = -ndc.y * tan(fovY * 0.5);

    // Ray direction in world space
    float3 dir = normalize(forward + px * right + py * camUp);

    // Build ray
    RayDesc ray;
    ray.Origin = camPos;
    ray.Direction = dir;
    ray.TMin = kMinT;
    ray.TMax = kMaxT;

    // Payload init
    HitPayload payload;
    payload.color = float3(0, 0, 0);
    payload.weight = 1;
    payload.distance = -1.0f;
    payload.depth = 0;

    TraceRay(topLevelAS, 0, 0xff, 0, 0, 0, ray, payload);

    float3 color = float3(payload.distance / kMaxT);

    if (payload.depth == 0)
        return;

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