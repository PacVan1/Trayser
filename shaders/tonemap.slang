enum TonemapMode
{
	None,
	Reinhard,
	ACES,
    PBRNeutral
};

static constexpr float kGamma = 2.2;
static constexpr float kInvGamma = 1.0 / 2.2;
static constexpr float kPi = 3.14159265359;

[[vk::push_constant]]
cbuffer PushConstants
{
    [[vk::offset(0)]] int4 tonemapMode;
};

[[vk::binding(0, 0)]] RWTexture2D<float4> srcImage;
[[vk::binding(1, 0)]] RWTexture2D<float4> dstImage;

float3 TonemapACES_Narkowicz(float3 color)
{
    const float A = 2.51;
    const float B = 0.03;
    const float C = 2.43;
    const float D = 0.59;
    const float E = 0.14;
    return clamp((color * (A * color + B)) / (color * (C * color + D) + E), 0.0, 1.0);
}

float3 TonemapReinhard_Simple(float3 color)
{
    return color / (color + 1.0);
}

float3 TonemapKhronos_PBRNeutral(float3 color)
{
    const float startCompression = 0.8 - 0.04;
    const float desaturation = 0.15;

    float x = min(color.r, min(color.g, color.b));
    float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
    color -= offset;

    float peak = max(color.r, max(color.g, color.b));
    if (peak < startCompression) return color;

    const float d = 1. - startCompression;
    float newPeak = 1. - d * d / (peak + d - startCompression);
    color *= newPeak / peak;

    float g = 1. - 1. / (desaturation * (peak - newPeak) + 1.);
    return lerp(color, newPeak * float3(1, 1, 1), g);
}

float3 TonemapFilmic(float3 color)
{
    color = max(0, color - 0.004f);
    color = (color * (6.2f * color + 0.5f)) / (color * (6.2f * color + 1.7f) + 0.06f);
    return color;
}

[shader("compute")]
[numthreads(16, 16, 1)]
void csMain(uint3 dtID: SV_DispatchThreadID)    
{
    switch (tonemapMode.x)
    {
    case TonemapMode.None: break;

    case TonemapMode.Reinhard:
        dstImage[dtID.xy] = float4(TonemapReinhard_Simple(srcImage[dtID.xy].rgb), 1.0);
        break;

    case TonemapMode.ACES:
        dstImage[dtID.xy] = float4(TonemapACES_Narkowicz(srcImage[dtID.xy].rgb), 1.0);
        break;

    case TonemapMode.PBRNeutral:
        dstImage[dtID.xy] = float4(TonemapKhronos_PBRNeutral(srcImage[dtID.xy].rgb), 1.0);
        break;

    default: break;
    }
}