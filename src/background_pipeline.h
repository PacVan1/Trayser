#pragma once

#include <vulkan/vulkan.h>
#include <pipelines.h>

namespace trayser
{

class BackgroundPipeline final : public Pipeline
{
public:
    BackgroundPipeline();
    virtual void Load() override;
    virtual void Update() override;
};

} // namespace trayser
