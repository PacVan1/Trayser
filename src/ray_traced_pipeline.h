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
    };

public:
    RayTracedPipeline();
    virtual void Load() override;
    virtual void Update() override;

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
