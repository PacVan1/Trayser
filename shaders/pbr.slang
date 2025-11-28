struct Vertex
{
    [[vk::location(0)]] float3  position;
    [[vk::location(1)]] float   uvX;
    [[vk::location(2)]] float3  normal;
    [[vk::location(3)]] float   uvY;
    [[vk::location(4)]] float4  color;
    [[vk::location(5)]] float4  tangent;
};

struct PBRInfo
{
    float3 worldPos;
    float3 N; // Normal
    float3 V; // View
    float3 R; // Reflection
    float3 F0;
    float4 baseColor;
    float3 emissive;
    float metallic;
    float roughness;
    float ao;
};

struct VsFsInterface
{
    float4 position : SV_Position;
    float4 worldPos : WORLD_POS;
    float3 color    : TEXCOORD0;
    float2 uv       : TEXCOORD1;
    float3x3 tbn    : TEXCOORD2;
};

[[vk::push_constant]]
cbuffer PushConstants
{
    [[vk::offset(0)]]   column_major float4x4 viewProj;
    [[vk::offset(64)]]  column_major float4x4 model;
    [[vk::offset(128)]] float4 camPos;
    [[vk::offset(144)]] int4 renderMode;
};

// Textures with explicit Vulkan bindings
//[[vk::binding(0, 0)]] Sampler2D textures[];
[[vk::binding(0, 0)]] Sampler2D baseColorTexture;
[[vk::binding(1, 0)]] Sampler2D normalMapTexture;
[[vk::binding(2, 0)]] Sampler2D metallicRoughnessTexture;
[[vk::binding(3, 0)]] Sampler2D occlusionTexture;
[[vk::binding(4, 0)]] Sampler2D emissiveTexture;

float3x3 inverse(float3x3 m)
{
    float det = determinant(m);
    // Compute adjugate and divide by det
    return transpose(float3x3(
        cross(m[1], m[2]),
        cross(m[2], m[0]),
        cross(m[0], m[1])
    )) / det;
}

[shader("vertex")]
VsFsInterface vsMain(Vertex v)
{
    VsFsInterface o;

    // Transform
    float4x4 mvp = mul(viewProj, model);
    o.position = mul(mvp, float4(v.position, 1.0f));
    o.worldPos = mul(model, float4(v.position, 1.0f));

    // Outputs
    o.color = v.color.xyz;
    o.uv = float2(v.uvX, v.uvY);

    float3 normal   = normalize(mul(transpose(inverse((float3x3)model)), v.normal));
    float3 tangent  = normalize(mul((float3x3)model, v.tangent.xyz));
    tangent         = normalize(tangent - normal * dot(normal, tangent));
    float3 bitangent = cross(normal, tangent) * v.tangent.w;

    o.tbn = float3x3(tangent, bitangent, normal);

    return o;
}

static constexpr float kGamma = 2.2;
static constexpr float kInvGamma = 1.0 / 2.2;
static constexpr float kPi = 3.14159265359;

enum TextureType
{
    BaseColor,
    NormalMap,
    MetallicRoughness,
    AmbientOcclusion,
    Emission,
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
    WorldPos,
    ViewDir,
    Wireframe,
    Depth,
};

float SrgbToLinear(float color)
{
    return pow(color, kGamma);
}

float2 SrgbToLinear(float2 color)
{
    return pow(color, kGamma);
}

float3 SrgbToLinear(float3 color)
{
    return pow(color, kGamma);
}

float4 SrgbToLinear(float4 color)
{
    return pow(color, kGamma);
}

float LinearToSrgb(float color)
{
    return pow(color, kInvGamma);
}

float2 LinearToSrgb(float2 color)
{
    return pow(color, kInvGamma);
}

float3 LinearToSrgb(float3 color)
{
    return pow(color, kInvGamma);
}

float4 LinearToSrgb(float4 color)
{
    return pow(color, kInvGamma);
}

float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max(float3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float DistributionGGX(float3 N, float3 H, float roughness)
{
    // src: https://learnopengl.com/PBR/Lighting
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = kPi * denom * denom;
    return a2 / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    // src: https://learnopengl.com/PBR/Lighting
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    // src: https://learnopengl.com/PBR/Lighting
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

float3 Lambert(float3 albedo)
{
    return albedo / kPi;
}

float3 CookTorranceBRDF(PBRInfo info, float3 L)
{
    float3 H = normalize(info.V + L);

    float NDF = DistributionGGX(info.N, H, info.roughness);
    float G = GeometrySmith(info.N, info.V, L, info.roughness);
    float3 F = FresnelSchlickRoughness(clamp(dot(H, info.V), 0.0, 1.0), info.F0, info.roughness);

    float3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(info.N, info.V), 0.0) * max(dot(info.N, L), 0.0) + 0.0001;
    float3 specular = numerator / denominator;

    float3 kS = F;
    float3 kD = float3(1.0) - kS;
    kD *= 1.0 - info.metallic;

    float NdotL = max(dot(info.N, L), 0.0);

    return (kD * Lambert(info.baseColor.rgb) + specular) * NdotL;
}

// Fragment shader output
struct FSOutput
{
    float4 fragColor : SV_Target0;
};

[shader("fragment")]
FSOutput fsMain(VsFsInterface input)
{
    FSOutput o;

    float3 L1 = normalize(float3(0.491, 0.085, 0.153));
    float3 L2 = -L1;

    //PBRInfo info;
    //info.worldPos = input.worldPos.xyz;
    //float3 mapNormal = normalize(textures[TextureType.NormalMap].Sample(input.uv).xyz * 2.0 - 1.0);
    //info.N = normalize(mul(mapNormal, input.tbn));
    //
    //info.baseColor = textures[TextureType.BaseColor].Sample(input.uv);
    //info.emissive = textures[TextureType.Emission].Sample(input.uv).rgb;
    //float3 metallicRoughness = textures[TextureType.MetallicRoughness].Sample(input.uv).rgb;
    //info.metallic = metallicRoughness.b;
    //info.roughness = metallicRoughness.g;
    //info.ao = textures[TextureType.AmbientOcclusion].Sample(input.uv).r;

    PBRInfo info;
    info.worldPos = input.worldPos.xyz;
    float3 mapNormal = normalize(normalMapTexture.Sample(input.uv).xyz * 2.0 - 1.0);
    info.N = normalize(mul(mapNormal, input.tbn));

    info.baseColor = baseColorTexture.Sample(input.uv);
    info.emissive = emissiveTexture.Sample(input.uv).rgb;
    float3 metallicRoughness = metallicRoughnessTexture.Sample(input.uv).rgb;
    info.metallic = metallicRoughness.b;
    info.roughness = metallicRoughness.g;
    info.ao = occlusionTexture.Sample(input.uv).r;

    info.F0 = float3(0.04);
    info.F0 = lerp(info.F0, info.baseColor.rgb, info.metallic);
    info.V = normalize(camPos.xyz - info.worldPos.xyz);
    info.R = reflect(info.V, info.N);

    switch (renderMode.x)
    {
    case RenderMode.FinalColor:
        float3 intensity = 0.0;
        intensity += CookTorranceBRDF(info, L1) * 2.0;
        intensity += CookTorranceBRDF(info, L2) * 2.0;
        o.fragColor = float4(intensity * info.ao + info.emissive, 1.0);
        o.fragColor = float4(LinearToSrgb(o.fragColor.rgb), 1.0);
        break;

    case RenderMode.TexCoord0:
        o.fragColor = float4(input.uv, 0.0, 1.0);
        break;

    case RenderMode.Tangent:
        o.fragColor = float4((normalize(input.tbn[0]) + 1.0) * 0.5, 1.0);
        break;

    case RenderMode.Bitangent:
        o.fragColor = float4((normalize(input.tbn[1]) + 1.0) * 0.5, 1.0);
        break;

    case RenderMode.GeometryNormal:
        o.fragColor = float4((normalize(input.tbn[2]) + 1.0) * 0.5, 1.0);
        break;

    case RenderMode.ShadingNormal:
        o.fragColor = float4((info.N + 1.0) * 0.5, 1.0);
        break;

    case RenderMode.BaseColor:
        o.fragColor = float4(LinearToSrgb(info.baseColor.rgb), 1.0);
        break;

    case RenderMode.NormalMap:
        o.fragColor = normalMapTexture.Sample(input.uv);
        break;

    case RenderMode.Metallic:
        o.fragColor = float4(info.metallic, info.metallic, info.metallic, 1.0);
        break;

    case RenderMode.Roughness:
        o.fragColor = float4(info.roughness, info.roughness, info.roughness, 1.0);
        break;

    case RenderMode.AmbientOcclusion:
        o.fragColor = float4(info.ao, info.ao, info.ao, 1.0);
        break;

    case RenderMode.Emission:
        o.fragColor = float4(LinearToSrgb(info.emissive), 1.0);
        break;

    case RenderMode.WorldPos:
        o.fragColor = float4(input.worldPos.xyz, 1.0);
        break;

    case RenderMode.ViewDir:
        o.fragColor = float4((info.V.xyz + 1.0) * 0.5, 1.0);
        break;

    default:
        o.fragColor = float4(1.0, 0.0, 1.0, 1.0); // Magenta for unsupported modes
        break;
    }

    return o;
}