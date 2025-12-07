#pragma once

#include <vulkan/vulkan.h>
#include <pipelines.h>

namespace trayser
{

class RasterizedPipeline final : public Pipeline
{
public:
    RasterizedPipeline();
    virtual void Load() override;
    virtual void Update() override;
};

} // namespace trayser
