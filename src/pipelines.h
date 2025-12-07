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

} // namespace trayser

#include <background_pipeline.h>
#include <tonemap_pipeline.h>
#include <rasterized_pipeline.h>
#include <ray_traced_pipeline.h>