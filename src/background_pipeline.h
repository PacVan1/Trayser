#pragma once

#include <vulkan/vulkan.h>
#include <pipelines.h>

namespace trayser
{

class BackgroundPipeline final : public Pipeline
{
public:
    BackgroundPipeline();
    virtual void Load(VkShaderModule module) override;
    virtual void Update() override;
    virtual VkShaderModule Compile() override;
};

} // namespace trayser
