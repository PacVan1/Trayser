#include "../src/gpu_io.h"

[[vk::binding(0, 0)]] Sampler2D srcImage;
[[vk::binding(1, 0)]] RWTexture2DArray<float4> dstCubemap;

[[vk::push_constant]]
cbuffer PushConstants
{
    [[vk::offset(0)]] uint32_t faceSize;
};

float3 directionFromFaceUV(uint face, float2 uv)
{
    // convert [0,1] -> [-1,1]
    float2 xy = uv * 2.0 - 1.0;

    switch (face)
    {
    case 0: return normalize(float3(1.0, xy.y, -xy.x)); // +X
    case 1: return normalize(float3(-1.0, xy.y, xy.x)); // -X
    case 2: return normalize(float3(xy.x, 1.0, -xy.y)); // +Y
    case 3: return normalize(float3(xy.x, -1.0, xy.y)); // -Y
    case 4: return normalize(float3(xy.x, xy.y, 1.0)); // +Z
    case 5: return normalize(float3(xy.x, xy.y, -1.0)); // -Z
    }

    return float3(0, 0, 0); // should not happen
}

[shader("compute")]
[numthreads(16, 16, 1)]
void csMain(uint3 dtID: SV_DispatchThreadID)
{
    uint x = dtID.x;
    uint y = dtID.y;
    uint face = dtID.z;

    if (x >= faceSize || y >= faceSize)
        return;

    float2 uv = (float2(x, y) + 0.5) / float2(faceSize, faceSize);

    float3 dir = directionFromFaceUV(face, uv);

    float phi = atan2(dir.z, dir.x); // [-pi, pi]
    float theta = acos(dir.y);       // [0, pi]

    float2 equiUV;
    equiUV.x = phi / (2.0 * kPi) + 0.5;
    equiUV.y = theta / kPi;

    float3 color = srcImage.Sample(equiUV).rgb;

    dstCubemap[uint3(x, y, face)] = float4(color, 1.0);
}