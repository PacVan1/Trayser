#pragma once

#include <vulkan/vulkan.h>
#include <pipelines.h>

namespace trayser
{

class RasterizedPipeline final : public Pipeline
{
public:
    RasterizedPipeline();
    virtual void Load(VkShaderModule module) override;
    virtual void Update() override;
    virtual VkShaderModule Compile() override;
};

} // namespace trayser
