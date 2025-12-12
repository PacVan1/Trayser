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
	VkFormat		format;
	uint32_t		frameCount;
	uint32_t		imageIndex;

	std::vector<VkImage>	 images;
	std::vector<VkImageView> views;
};

struct PerFrame
{
	VkCommandBuffer cmdBuffer;
	VkDescriptorSet textureDescSet;
	VkSemaphore     acquisitionSemaphore;	// Wait for image to be available
	VkSemaphore		renderedSemaphore;		// Wait for rendering to be complete
	VkFence			renderedFence;
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

	[[nodiscard]] VkCommandBuffer	GetCmdBuffer() const			{ return m_frames[m_frameIndex].cmdBuffer; }
	[[nodiscard]] VkDescriptorSet	GetTextureDescSet() const		{ return m_frames[m_frameIndex].textureDescSet; }
	[[nodiscard]] VkImage			GetSwapchainImage() const		{ return m_swapchain.images[m_swapchain.imageIndex]; }
	[[nodiscard]] VkImageView		GetSwapchainImageView() const	{ return m_swapchain.views[m_swapchain.imageIndex]; }
	[[nodiscard]] VkSemaphore		GetAcquisitionSemaphore() const	{ return m_frames[m_frameIndex].acquisitionSemaphore; }
	[[nodiscard]] VkSemaphore		GetRenderedSemaphore() const	{ return m_frames[m_frameIndex].renderedSemaphore; }
	[[nodiscard]] VkFence			GetRenderedFence() const		{ return m_frames[m_frameIndex].renderedFence; }
	[[nodiscard]] const PerFrame&	GetFrame() const				{ return m_frames[m_frameIndex]; }
	[[nodiscard]] PerFrame&			GetFrame()						{ return m_frames[m_frameIndex]; }

private:
	void InitCmdPool(Device& device);
	void InitSwapchain(Device& device);
	void InitFrames(Device& device);
	void InitCmdBuffers(Device& device);
	void InitSyncStructures(Device& device);
	void InitTextureDescLayout(Device& device);
	void InitDefaultImage(Device& device);
	void InitTextureDescSets(Device& device);
	void InitImGui(Device& device);

	void DestroyCmdPool(Device& device) const;
	void DestroySwapchain(Device& device) const;
	void DestroyFrames(Device& device) const;
	void DestroySyncStructures(Device& device) const;
	void DestroyDefaultImage(Device& device) const;
	void DestroyTextureDescSets(Device& device) const;
	void DestroyImGui() const;

	[[nodiscard]] VkSurfaceFormatKHR	ChooseSwapchainFormat(const std::vector<VkSurfaceFormatKHR>& formats);
	[[nodiscard]] VkPresentModeKHR		ChoosePresentMode(const std::vector<VkPresentModeKHR>& presentModes);
	[[nodiscard]] VkExtent2D			ChooseSwapchainExtent(const Device& device, const VkSurfaceCapabilitiesKHR& capabilities);

	void ImGuiNewFrame();
	void RenderImGui();
	void AdvanceFrame();

public:
	static constexpr uint32_t kMaxFramesInFlight = 2;

public:
	Swapchain				m_swapchain;
	VkCommandPool			m_cmdPool;
	VkDescriptorSetLayout	m_textureDescLayout;
	PerFrame				m_frames[kMaxFramesInFlight];
	DefaultImage			m_defaultImage; // 1x1 black image to initialize
	uint32_t				m_frameCounter;
	uint32_t				m_frameIndex;
};

} // namespace trayser
