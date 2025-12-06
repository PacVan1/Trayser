#pragma once 

#include <types.h>
#include <chrono>
#include <slang/slang.h>
#include <slang/slang-com-ptr.h>
#include <vector>
#include <device.h>

namespace trayser
{

class Pipeline
{
public:
	std::chrono::time_point <std::chrono::file_clock> m_lastWriteTime;
    std::string         m_name;
    VkPipelineLayout    m_layout;
    VkPipeline          m_pipeline;
    bool                m_canHotReload = false;

public:
    void Init();
    void Destroy() const;
    void ReloadIfChanged();

    virtual void Load() = 0;
    virtual void Update() = 0;
};

class PBRPipeline final : public Pipeline
{
public:
    PBRPipeline();
    virtual void Load() override;
    virtual void Update() override;
};

class BackgroundPipeline final : public Pipeline
{
public:
    BackgroundPipeline();
    virtual void Load() override;
    virtual void Update() override;
};

class TonemapPipeline final : public Pipeline
{
public:
    struct PushConstantComp
    {
        int tonemapMode;
        int padding[3];
    };

public:
    TonemapPipeline();
    virtual void Load() override;
    virtual void Update() override;

private:
	VkDescriptorSetLayout m_descriptorSetLayout;
    VkDescriptorSet       m_descriptorSet;
};

class RayTracingPipeline final : public Pipeline
{
public:
    enum BindingPoints
    {
        BindingPoints_TLAS,
        BindingPoints_OutImage,
    };

public:
    RayTracingPipeline();
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