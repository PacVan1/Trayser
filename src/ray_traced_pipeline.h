#pragma once

#include <vulkan/vulkan.h>
#include <pipelines.h>

namespace trayser
{

class RayTracedPipeline final : public Pipeline
{
public:
    enum BindingPoints
    {
        BindingPoints_TLAS,
        BindingPoints_OutImage,
        BindingPoints_OutAccumulator,
    };

public:
    RayTracedPipeline();
    virtual void Load(VkShaderModule module) override;
    virtual void Update() override;
    virtual VkShaderModule Compile() override;

    void PipelineBarrier() const;
    void ClearIfAccumulatorReset();

private:
    VkDescriptorSetLayout   m_descriptorSetLayout;
    VkDescriptorSet         m_descriptorSet;
    std::vector<u8>         m_shaderHandles;
    Buffer                  m_sbtBuffer;
    
    VkStridedDeviceAddressRegionKHR m_raygenRegion;    // Ray generation shader region
    VkStridedDeviceAddressRegionKHR m_missRegion;      // Miss shader region
    VkStridedDeviceAddressRegionKHR m_hitRegion;       // Hit shader region
    VkStridedDeviceAddressRegionKHR m_callableRegion;  // Callable shader region
};

} // namespace trayser
