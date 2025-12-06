#pragma once

#include <vulkan/vulkan.h>

#include <pipelines.h>

namespace trayser
{

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

} // namespace trayser
