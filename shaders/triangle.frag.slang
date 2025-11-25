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
//[[vk::binding(0, 0)]] Sampler2D baseColor;
//[[vk::binding(1, 0)]] Sampler2D normalMap;
//[[vk::binding(2, 0)]] Sampler2D metallicRoughness;
//[[vk::binding(3, 0)]] Sampler2D occlusion;
//[[vk::binding(4, 0)]] Sampler2D emissive;

[[vk::binding(0, 0)]] Sampler2D textures[];

// Push constants block
[[vk::push_constant]]
cbuffer PushConstants
{
    int4 renderMode;
};

enum TextureType
{
    BaseColor,
    NormalMap,
    MetallicRoughness,
    Occlusion,
    Emissive,
}

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

    switch (renderMode.x)
    {
        case RenderMode.FinalColor:
            o.fragColor = float4(0.5, 0.5, 0.5, 1.0);
            break;

        case RenderMode.TexCoord0:
            o.fragColor = float4(input.uv, 0.0, 1.0);
            break;

        case RenderMode.Tangent:
            o.fragColor = float4((input.tbn[0] + 1.0) * 0.5, 1.0);
            break;

        case RenderMode.Bitangent:
            o.fragColor = float4((input.tbn[1] + 1.0) * 0.5, 1.0);
            break;

        case RenderMode.GeometryNormal:
            o.fragColor = float4((input.tbn[2] + 1.0) * 0.5, 1.0);
            break;

        case RenderMode.ShadingNormal:
            float3 mapNormal = textures[TextureType.NormalMap].Sample(input.uv).xyz * 2.0 - 1.0;
            float3 normal = mul(input.tbn, mapNormal);
            o.fragColor = float4((normal + 1.0) * 0.5, 1.0);
            break;

        case RenderMode.BaseColor:
            o.fragColor = textures[TextureType.BaseColor].Sample(input.uv);
            break;

        case RenderMode.NormalMap:
            o.fragColor = textures[TextureType.NormalMap].Sample(input.uv);
            break;

        case RenderMode.Metallic:
            float metallic = textures[TextureType.MetallicRoughness].Sample(input.uv).b;
            o.fragColor = float4(metallic, metallic, metallic, 1.0);
            break;

        case RenderMode.Roughness:
            float roughness = textures[TextureType.MetallicRoughness].Sample(input.uv).g;
            o.fragColor = float4(roughness, roughness, roughness, 1.0);
            break;

        case RenderMode.AmbientOcclusion:
            o.fragColor = textures[TextureType.Occlusion].Sample(input.uv);
            break;

        case RenderMode.Emission:
            o.fragColor = textures[TextureType.Emissive].Sample(input.uv);
            break;
    }

    return o;
}