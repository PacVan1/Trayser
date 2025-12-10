#include "../src/gpu_io.h"

[[vk::push_constant]] ConstantBuffer<RasterPushConstants> pushConsts;
[[vk::binding(0, 0)]] Sampler2D textures[100];
[[vk::binding(1, 0)]] SamplerCube skybox;

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
    float4 uv : TEXCOORD1;
    float3x3 tbn : TEXCOORD2;
};

[shader("vertex")]
VsFsInterface vsMain(Vertex v)
{
    VsFsInterface o;

    Scene scene = pushConsts.sceneRef[0];
    Camera camera = scene.camera;
    REF(Instance) instanceBufferRef = scene.instanceBufferRef;
    Instance instance = instanceBufferRef[pushConsts.instanceIdx];

    // Transform
    float4x4 mvp = mul(camera.viewProj, instance.transform);
    o.position = mul(mvp, float4(v.position.xyz, 1.0));
    o.worldPos = mul(instance.transform, float4(v.position.xyz, 1.0));

    // Outputs
    o.uv = v.texCoord;

    float3 normal = normalize(mul(instance.normalTransform, v.normal.xyz));
    float3 tangent = normalize(mul((float3x3)instance.transform, v.tangent.xyz));
    tangent         = normalize(tangent - normal * dot(normal, tangent));
    float3 bitangent = cross(normal, tangent) * v.tangent.w;

    o.tbn = float3x3(tangent, bitangent, normal);

    return o;
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
    Scene scene = pushConsts.sceneRef[0];
    Camera camera = scene.camera;
    REF(Mesh) meshBufferRef = scene.meshBufferRef;
    REF(Instance) instanceBufferRef = scene.instanceBufferRef;
    Instance instance = instanceBufferRef[pushConsts.instanceIdx];
    Mesh mesh = meshBufferRef[instance.meshHandle];
    REF(Vertex) vertexBufferRef = mesh.vertexBufferRef;
    REF(uint32_t) indexBufferRef = mesh.indexBufferRef;
    REF(Material) materialBufferRef = scene.materialBufferRef;
    REF(Primitive) primitiveBufferRef = mesh.primitiveBufferRef;
    Material material = materialBufferRef[primitiveBufferRef[0].materialHandle];

    Sampler2D baseColorTexture  = textures[material.baseColorHandle];
    Sampler2D normalMapTexture  = textures[material.normalMapHandle];
    Sampler2D metalRoughTexture = textures[material.metalRoughHandle];
    Sampler2D aoTexture         = textures[material.aoHandle];
    Sampler2D emissiveTexture   = textures[material.emissiveHandle];

    FSOutput o;

    PBRInfo info;
    info.worldPos = input.worldPos.xyz;
    float3 mapNormal = normalize(normalMapTexture.Sample(input.uv.xy).xyz * 2.0 - 1.0);
    info.N = normalize(mul(mapNormal, input.tbn));

    info.baseColor = baseColorTexture.Sample(input.uv.xy);
    info.emissive = emissiveTexture.Sample(input.uv.xy).rgb;
    float3 metallicRoughness = metalRoughTexture.Sample(input.uv.xy).rgb;
    info.metallic = metallicRoughness.b;
    info.roughness = metallicRoughness.g;
    info.ao = aoTexture.Sample(input.uv.xy).r;

    info.F0 = float3(0.04);
    info.F0 = lerp(info.F0, info.baseColor.rgb, info.metallic);
    info.V = normalize(GetPosition(camera) - info.worldPos.xyz);
    info.R = reflect(info.V, info.N);

    switch (pushConsts.renderMode)
    {
    case RenderMode.FinalColor:
        float3 intensity = 0.0;
        float3 L1 = normalize(float3(0.491, 0.085, 0.153));
        float3 L2 = -L1;
        intensity += CookTorranceBRDF(info, L1) * 2.0;
        intensity += CookTorranceBRDF(info, L2) * 2.0;
        o.fragColor = float4(intensity * info.ao + info.emissive, 1.0);
        o.fragColor = float4(LinearToSrgb(o.fragColor.rgb), 1.0);
        break;

    case RenderMode.TexCoord0:
        o.fragColor = float4(input.uv.xy, 0.0, 1.0);
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
        o.fragColor = normalMapTexture.Sample(input.uv.xy);
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