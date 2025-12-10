#pragma once 

#include <types.h>
#include <chrono>
#include <vector>
#include <device.h>

namespace trayser
{

class Pipeline
{
public:
    using file_clock = std::chrono::time_point <std::chrono::file_clock>;

    std::string         m_name;
	file_clock          m_lastWriteTime;
    VkPipelineLayout    m_layout;
    VkPipeline          m_pipeline;
    bool                m_canHotReload = false;

public:
    void Init();
    void Destroy() const;
    void ReloadIfChanged();

    virtual void Load(VkShaderModule module) = 0;
    virtual void Update() = 0;
    virtual VkShaderModule Compile() = 0;
};

} // namespace trayser

#include <background_pipeline.h>
#include <tonemap_pipeline.h>
#include <rasterized_pipeline.h>
#include <ray_traced_pipeline.h>
#include <equi2cubemap_pipeline.h>