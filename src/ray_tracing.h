#pragma once

#include <pipelines.h>
#include <loader.h>

namespace trayser
{

class RayTracingPipeline : public Pipeline
{
public:
    virtual void Load() override;
    virtual void Update() override;
};

} // namespace trayser
