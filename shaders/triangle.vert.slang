struct Vertex
{
    float3 position;
    float  uv_x;
    float3 normal;
    float  uv_y;
    float4 color;
    float4 tangent;
};

[[vk::push_constant]]
cbuffer PushConstants
{
    float4x4 viewProj;
    float4x4 model;
};

// Vertex shader output
struct VSOutput
{
    float4 position : SV_Position;
    float3 color    : TEXCOORD0;
    float2 uv       : TEXCOORD1;
    float3x3 tbn    : TEXCOORD2;
};

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

VSOutput main(Vertex v)
{
    VSOutput o;

    // Transform
    float4x4 mvp = mul(viewProj, model);
    o.position = mul(mvp, float4(v.position, 1.0f));

    // Outputs
    o.color = v.color.xyz;
    o.uv = float2(v.uv_x, v.uv_y);

    float3 normal   = normalize(mul(transpose(inverse((float3x3)model)), v.normal));
    float3 tangent  = normalize(mul((float3x3)model, v.tangent.xyz));
    tangent         = normalize(tangent - normal * dot(normal, tangent));
    float3 bitangent = cross(normal, tangent) * v.tangent.w;

    o.tbn = float3x3(tangent, bitangent, normal);

    return o;
}