#pragma once

#include <vk_types.h>
#include <util.h>
#include <vk_descriptors.h>

constexpr unsigned int kFrameCount{ 2 };

struct ComputePushConstants 
{
	glm::vec4 data1;
	glm::vec4 data2;
	glm::vec4 data3;
	glm::vec4 data4;
};

struct FrameData
{
	VkCommandPool	commandPool;
	VkCommandBuffer commandBuffer;
	VkSemaphore     swapchainSemaphore, renderSemaphore;
	VkFence			renderFence;

	vkutil::DeletionQueue deletionQueue;
};

class VulkanEngine 
{
public:
	static VulkanEngine& Get();

public:
	void Init();
	void Cleanup();
	void Render();
	void Run();
	void ImmediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);

	[[nodiscard]] FrameData& GetCurrentFrame() { return m_frames[m_frameIdx]; }

private:
	void InitVulkan();
	void InitSwapchain();
	void InitCommands();
	void InitSyncStructures();
	void InitDescriptors();
	void InitPipelines();
	void InitBackgroundPipelines();
	void CreateSwapchain(u32 width, u32 height);
	void DestroySwapchain();
	void BeginRecording(VkCommandBuffer cmd);
	void RenderBackground(VkCommandBuffer cmd);

public:
	// Immediate submit structures
	VkFence						m_immFence;
	VkCommandBuffer				m_immCommandBuffer;
	VkCommandPool				m_immCommandPool;

	VkPipeline					m_gradientPipeline;
	VkPipelineLayout			m_gradientPipelineLayout;

	DescriptorAllocator			m_globalDescriptorAllocator;
	VkDescriptorSet				m_renderImageDescriptors;
	VkDescriptorSetLayout		m_renderImageDescriptorLayout;

	AllocatedImage				m_renderImage;
	VkExtent2D					m_renderExtent;

	VmaAllocator				m_allocator;
	vkutil::DeletionQueue		m_deletionQueue;

	VkInstance					m_instance;			// Vulkan library handle
	VkDebugUtilsMessengerEXT	m_debugMessenger;	// Vulkan debug output handle
	VkPhysicalDevice			m_chosenGPU;		// GPU chosen as the default device
	VkDevice					m_device;			// Vulkan device for commands
	VkSurfaceKHR				m_surface;			// Vulkan window surface

	VkSwapchainKHR				m_swapchain;
	VkFormat					m_swapchainImageFormat;
	std::vector<VkImage>		m_swapchainImages;
	std::vector<VkImageView>	m_swapchainImageViews;
	VkExtent2D					m_swapchainExtent;

	VkQueue						m_graphicsQueue;
	u32							m_graphicsQueueFamily;
	u32							m_frameIdx{0};

	bool						m_isInitialized{false};
	bool						m_stopRendering{false};
	VkExtent2D					m_windowExtent{1700, 900};
	struct SDL_Window*			m_window{nullptr};

private:
	FrameData					m_frames[kFrameCount];

};
