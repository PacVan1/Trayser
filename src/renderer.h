#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>

#include <device.h>

namespace trayser
{

struct PerFrame
{
	VkCommandBuffer cmdBuffer;
	VkDescriptorSet textureDescSet;
};

class Renderer
{
public:
	void Init(Device& device);
	void Destroy(Device& device);
	void Render(uint32_t frameIndex);

private:
	void InitCmdPool(Device& device);
	void InitTextureDescLayout(Device& device);
	void InitFrames(Device& device);
	void InitCmdBuffers(Device& device);
	void InitTextureDescSets(Device& device);

	void DestroyCmdPool(Device& device) const;

public:
	VkCommandPool			m_cmdPool;
	VkDescriptorSetLayout	m_textureDescLayout;
	PerFrame				m_frames[kFrameCount];

};

} // namespace trayser
