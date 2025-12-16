#ifdef __SLANG__
float MisWeightPower(in float a, in float b)
{
    float a2 = a * a;
    float b2 = b * b;
    float a2b2 = a2 + b2;
    return a2 / a2b2;
}
float MisWeightBalance(in float a, in float b)
{
    float ab = a + b;
    return a / ab;
}
float GetBrdfProbability(MaterialProperties material, float3 V, float3 shadingNormal) 
{
    // Evaluate Fresnel term using the shading normal
    // Note: we use the shading normal instead of the microfacet normal (half-vector) for Fresnel term here. That's suboptimal for rough surfaces at grazing angles, but half-vector is yet unknown at this point
    float specularF0 = luminance(baseColorToSpecularF0(material.baseColor, material.metalness));
    float diffuseReflectance = luminance(baseColorToDiffuseReflectance(material.baseColor, material.metalness));
    float Fresnel = saturate(luminance(evalFresnel(specularF0, shadowedF90(specularF0), max(0.0f, dot(V, shadingNormal)))));

    // Approximate relative contribution of BRDFs using the Fresnel term
    float specular = Fresnel;
    float diffuse = diffuseReflectance * (1.0f - Fresnel); //< If diffuse term is weighted by Fresnel, apply it here as well

    // Return probability of selecting specular BRDF over diffuse BRDF
    float p = (specular / max(0.0001f, (specular + diffuse)));

    // Clamp probability to avoid undersampling of less prominent BRDF
    return clamp(p, 0.1f, 0.9f);
}
float3 SampleBrdf(inout uint32_t rngState, in MaterialProperties material, in float3 V, in float3 N)
{
    int brdfType;
    if (material.metalness == 1.0f && material.roughness == 0.0f)
        brdfType = SPECULAR_TYPE;
    else 
    {   // Decide whether to sample diffuse or specular BRDF (based on Fresnel term)
        float brdfProbability = GetBrdfProbability(material, V, N);
        if (RandomFloat(rngState) < brdfProbability) 
        {
            brdfType = SPECULAR_TYPE;
            //payload.throughput /= brdfProbability;
        } 
        else 
        {
            brdfType = DIFFUSE_TYPE;
            //payload.throughput /= (1.0f - brdfProbability);
        }
    }
}

#endif