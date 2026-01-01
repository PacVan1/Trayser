#include <pch.h>
#include <pipelines.h>
#include <fstream>
//#include <initializers.h>
#include <engine.h>
#include <slang_compiler.h>
#include <filesystem>
#include <array>

void trayser::Pipeline::Init()
{
    if (m_canHotReload)
    {
        std::string filePath = g_engine.m_compiler.FindExistingFile((m_name + ".slang").c_str());
        if (filePath.empty())
            return;

        m_lastWriteTime = std::filesystem::last_write_time(filePath);
    }

    VkShaderModule module = Compile();
    if (module == VK_NULL_HANDLE)
        return;
    Load(module);
}

void trayser::Pipeline::Destroy() const
{
	vkDestroyPipelineLayout(g_engine.m_device.m_device, m_layout, nullptr);
	vkDestroyPipeline(g_engine.m_device.m_device, m_pipeline, nullptr);
}

void trayser::Pipeline::ReloadIfChanged()
{
	if (!m_canHotReload)
		return;

    std::string filePath = g_engine.m_compiler.FindExistingFile((m_name + ".slang").c_str());
	if (filePath.empty())
		return;

    auto currentLastWriteTime = std::filesystem::last_write_time(filePath);
    // Shader source has changed
    if (m_lastWriteTime < currentLastWriteTime)
    {   
        m_lastWriteTime = currentLastWriteTime;

        vkQueueWaitIdle(g_engine.m_device.m_graphicsQueue);
        VkShaderModule module = Compile();
        if (module == VK_NULL_HANDLE)
            return;
        Destroy();
        Load(module);
    }
}
