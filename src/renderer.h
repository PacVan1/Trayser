#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>

#include <device.h>

namespace trayser
{

struct Swapchain
{
	VkSwapchainKHR	swapchain;
	VkExtent2D		extent;
	uint32_t		frameCount;
	VkFormat		format;
};

struct PerFrame
{
	VkCommandBuffer cmdBuffer;
	VkDescriptorSet textureDescSet;
	VkImage			swapchainImage;
	VkImageView		swapchainImageView;
	VkSemaphore     swapchainSemaphore;
	VkSemaphore		renderSemaphore;
	VkFence			renderFence;
};

struct DefaultImage
{
	VkImage			image;
	VkImageView		view;
	VkSampler		sampler;
	VmaAllocation	allocation;
};

class Renderer
{
public:
	void Init(Device& device);
	void Destroy(Device& device);

	void Render(uint32_t frameIndex);
	void NewFrame(Device& device);
	void EndFrame(Device& device);

	VkCommandBuffer&	GetCmdBuffer() const			{ return m_frames[m_frameIndex].cmdBuffer; }
	VkDescriptorSet&	GetTextureDescSet() const		{ return m_frames[m_frameIndex].textureDescSet; }
	VkImage&			GetSwapchainImage() const		{ return m_frames[m_frameIndex].swapchainImage; }
	VkImageView&		GetSwapchainImageView() const	{ return m_frames[m_frameIndex].swapchainImageView; }
	VkSemaphore&		GetSwapchainSemaphore() const	{ return m_frames[m_frameIndex].swapchainSemaphore; }
	VkSemaphore&		GetRenderSemaphore() const		{ return m_frames[m_frameIndex].renderSemaphore; }
	VkFence&			GetRenderFence() const			{ return m_frames[m_frameIndex].renderFence; }

private:
	void InitCmdPool(Device& device);
	void InitSwapchain(Device& device);
	void InitFrames(Device& device);
	void InitCmdBuffers(Device& device);
	void InitSwapchainImages(Device& device);
	void InitSwapchainImageViews(Device& device);
	void InitSyncStructures(Device& device);
	void InitTextureDescLayout(Device& device);
	void InitDefaultImage(Device& device);
	void InitTextureDescSets(Device& device);
	void InitImGui(Device& device);

	void DestroyCmdPool(Device& device) const;
	void DestroySwapchain(Device& device) const;
	void DestroySwapchainImageViews(Device& device) const;
	void DestroyFrames(Device& device) const;
	void DestroySyncStructures(Device& device) const;
	void DestroyDefaultImage(Device& device) const;
	void DestroyTextureDescSets(Device& device) const;
	void DestroyImGui() const;

	VkSurfaceFormatKHR	ChooseSwapchainFormat(const std::vector<VkSurfaceFormatKHR>& formats);
	VkPresentModeKHR	ChoosePresentMode(const std::vector<VkPresentModeKHR>& presentModes);
	VkExtent2D			ChooseSwapchainExtent(const Device& device, const VkSurfaceCapabilitiesKHR& capabilities);

	void ImGuiNewFrame();
	void RenderImGui();

public:
	Swapchain				m_swapchain;
	VkCommandPool			m_cmdPool;
	VkDescriptorSetLayout	m_textureDescLayout;
	PerFrame*				m_frames;
	DefaultImage			m_defaultImage; // 1x1 black image to initialize
	uint32_t				m_frameCounter;
	uint32_t				m_frameIndex;
};

} // namespace trayser
