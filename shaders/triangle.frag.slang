// Fragment shader input from vertex stage
struct FSInput
{
    float4 position : SV_Position;
    float3 color    : TEXCOORD0;
    float2 uv       : TEXCOORD1;
    float3x3 tbn    : TEXCOORD2;
};

// Fragment shader output
struct FSOutput
{
    float4 fragColor : SV_Target0;
};

// Textures with explicit Vulkan bindings
[[vk::binding(0, 0)]] Sampler2D baseColor;
[[vk::binding(0, 1)]] Sampler2D normalMap;
[[vk::binding(0, 2)]] Sampler2D metallicRoughness;
[[vk::binding(0, 3)]] Sampler2D occlusion;
[[vk::binding(0, 4)]] Sampler2D emissive;

// Push constants block
[[vk::push_constant]]
cbuffer PushConstants
{
    int4 renderMode;
};

enum RenderMode
{
    FinalColor,
    TexCoord0,
    Tangent,
    Bitangent,
    GeometryNormal,
    ShadingNormal,
    BaseColor,
    NormalMap,
    Metallic,
    Roughness,
    AmbientOcclusion,
    Emission,
    Wireframe,
    Depth,
}

FSOutput main(FSInput input)
{
    FSOutput o;

    if (renderMode.x == 0)
    {
        o.fragColor = float4(0.5, 0.5, 0.5, 1.0);
    }
    else if (renderMode.x == RenderMode.TexCoord0)
    {
        o.fragColor = float4(input.uv, 0.0, 1.0);
    }
    else if (renderMode.x == RenderMode.Tangent)
    {
        o.fragColor = float4((input.tbn[0] + 1.0) * 0.5, 1.0);
    }
    else if (renderMode.x == RenderMode.Bitangent)
    {
        o.fragColor = float4((input.tbn[1] + 1.0) * 0.5, 1.0);
    }
    else if (renderMode.x == RenderMode.GeometryNormal)
    {
        o.fragColor = float4((input.tbn[2] + 1.0) * 0.5, 1.0);
    }
    else if (renderMode.x == RenderMode.ShadingNormal)
    {
        float3 mapNormal = normalMap.Sample(input.uv).xyz * 2.0 - 1.0;
        float3 normal    = mul(input.tbn, mapNormal);
        o.fragColor      = float4((normal + 1.0) * 0.5, 1.0);
    }
    else if (renderMode.x == RenderMode.BaseColor)
    {
        o.fragColor = baseColor.Sample(input.uv);
    }
    else if (renderMode.x == RenderMode.NormalMap)
    {
        o.fragColor = normalMap.Sample(input.uv);
    }
    else if (renderMode.x == RenderMode.Metallic)
    {
        float metallic = metallicRoughness.Sample(input.uv).b;
        o.fragColor    = float4(metallic, metallic, metallic, 1.0);
    }
    else if (renderMode.x == RenderMode.Roughness)
    {
        float roughness = metallicRoughness.Sample(input.uv).g;
        o.fragColor     = float4(roughness, roughness, roughness, 1.0);
    }
    else if (renderMode.x == RenderMode.AmbientOcclusion)
    {
        o.fragColor = occlusion.Sample(input.uv);
    }
    else if (renderMode.x == RenderMode.Emission)
    {
        o.fragColor = emissive.Sample(input.uv);
    }

    o.fragColor = float4((float)renderMode.x / 20.0, 0.0, 0.0, 1.0);

    return o;
}