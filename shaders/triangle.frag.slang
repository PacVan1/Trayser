// Fragment shader input from vertex stage
struct FSInput
{
    float4 position : SV_Position;
    float3 color : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float3x3 tbn : TEXCOORD2;
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
    int   renderMode;
    float3 padding; // matches GLSL layout
};

FSOutput main(FSInput input)
{
    FSOutput o;

    if (renderMode == 0)
    {
        o.fragColor = float4(0.5, 0.5, 0.5, 1.0);
    }
    else if (renderMode == 1)
    {
        o.fragColor = float4(input.uv, 0.0, 1.0);
    }
    else if (renderMode == 2)
    {
        o.fragColor = float4((input.tbn[0] + 1.0) * 0.5, 1.0);
    }
    else if (renderMode == 3)
    {
        o.fragColor = float4((input.tbn[1] + 1.0) * 0.5, 1.0);
    }
    else if (renderMode == 4)
    {
        o.fragColor = float4((input.tbn[2] + 1.0) * 0.5, 1.0);
    }
    else if (renderMode == 5)
    {
        float3 mapNormal = normalMap.Sample(input.uv).xyz * 2.0 - 1.0;
        float3 normal    = mul(input.tbn, mapNormal);
        o.fragColor      = float4((normal + 1.0) * 0.5, 1.0);
    }
    else if (renderMode == 6)
    {
        o.fragColor = baseColor.Sample(input.uv);
    }
    else if (renderMode == 7)
    {
        o.fragColor = normalMap.Sample(input.uv);
    }
    else if (renderMode == 8)
    {
        float metallic = metallicRoughness.Sample(input.uv).b;
        o.fragColor    = float4(metallic, metallic, metallic, 1.0);
    }
    else if (renderMode == 9)
    {
        float roughness = metallicRoughness.Sample(input.uv).g;
        o.fragColor     = float4(roughness, roughness, roughness, 1.0);
    }
    else if (renderMode == 10)
    {
        o.fragColor = occlusion.Sample(input.uv);
    }
    else if (renderMode == 11)
    {
        o.fragColor = emissive.Sample(input.uv);
    }

    return o;
}