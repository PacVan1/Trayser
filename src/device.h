#pragma once

#include <SDL.h>
#include <SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>

#include <types.h>
#include <input.h>
#include <descriptors.h>
#include <util.h>

#include <vector>
#include <functional>

namespace trayser
{

static constexpr char const* kEngineName	= "Trayser";
static constexpr u32 kFrameCount			= 2;
static constexpr u32 kInitWindowWidth		= 1700;
static constexpr u32 kInitWindowHeight		= 900;

struct QueueFamilyIndices
{
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;

	bool IsComplete() const
	{
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

struct QueueFamilies
{
	uint32_t graphics;
	uint32_t present;
};

struct FrameData
{
	VkCommandPool	commandPool;
	VkCommandBuffer commandBuffer;
	VkSemaphore     swapchainSemaphore, renderSemaphore;
	VkFence			renderFence;

	vkutil::DeletionQueue deletionQueue;
	DescriptorAllocatorGrowable descriptors;
};

struct SwapchainSupport
{
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

struct RuntimeFuncs
{
	void Init(); 

	PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
	PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
	PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
	PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
	PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;
	PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;
	PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;
	PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
};

class Device
{
public:
	struct Buffer
	{
		VkBuffer		buffer;
		VmaAllocation	allocation;
	};

	struct StageBuffer
	{
		VkBuffer		buffer;
		VmaAllocation	allocation;
		void*			mapped;
	};

	struct Image
	{
		VkExtent3D		extent;
		VkImage			image;
		VmaAllocation	allocation;
		VkFormat		format;
		uint32_t		mipLevels;
		uint32_t		arrayLayers;
	};

	struct AccelerationStructure
	{
		VkAccelerationStructureKHR  accel{};
		VkDeviceAddress             address{};
		Buffer						buffer;
	};

public:
	void Init();
	void Destroy();
	void ShowCursor(bool show);
	void BeginOneTimeSubmit(VkCommandBuffer& outCmd) const;
	VkCommandBuffer BeginOneTimeSubmit() const;
	void EndOneTimeSubmit() const;
	void NewFrame();
	void EndFrame();
	bool ShouldQuit() const;
	void CreateBottomLevelAs();
	void CreateBLas2();
	//void CreateTopLevelAs();

	SwapchainSupport GetSwapchainSupport() const;
	QueueFamilyIndices GetQueueFamilies();

	VkResult CreateImage(
		const VkImageCreateInfo& createInfo, 
		const VmaAllocationCreateInfo& allocCreateInfo, 
		Device::Image& outImage, 
		VmaAllocationInfo* outAllocInfo = nullptr) const;

	void DestroyImage(const Device::Image& image) const;

	VkResult CreateOneTimeStageBuffer(
		VkDeviceSize size,
		VkBufferUsageFlags bufferUsage,
		VmaAllocationCreateFlags allocFlags,
		Device::StageBuffer& outBuffer,
		VmaAllocationInfo* outAllocInfo = nullptr) const;

	VkResult CreateOneTimeStageBuffer(
		VkDeviceSize size,
		VkBufferUsageFlags bufferUsage,
		Device::StageBuffer& outBuffer,
		VmaAllocationInfo* outAllocInfo = nullptr) const;

	VkResult CreateOneTimeStageBuffer(
		VkDeviceSize size,
		Device::StageBuffer& outBuffer,
		VmaAllocationInfo* outAllocInfo = nullptr) const;

	VkResult CreateOneTimeStageBuffer(
		const VkBufferCreateInfo& bufferCreateInfo,
		const VmaAllocationCreateInfo& allocCreateInfo,
		Device::StageBuffer& outBuffer,
		VmaAllocationInfo* outAllocInfo = nullptr) const;

	VkResult CreateStageBuffer(
		VkDeviceSize size,
		VkBufferUsageFlags bufferUsage,
		VmaAllocationCreateFlags allocFlags,
		Device::StageBuffer& outBuffer,
		VmaAllocationInfo* outAllocInfo = nullptr) const;

	VkResult CreateStageBuffer(
		VkDeviceSize size,
		VkBufferUsageFlags bufferUsage,
		Device::StageBuffer& outBuffer,
		VmaAllocationInfo* outAllocInfo = nullptr) const;

	VkResult CreateStageBuffer(
		VkDeviceSize size,
		Device::StageBuffer& outBuffer,
		VmaAllocationInfo* outAllocInfo = nullptr) const;

	VkResult CreateStageBuffer(
		const VkBufferCreateInfo& bufferCreateInfo,
		const VmaAllocationCreateInfo& allocCreateInfo,
		Device::StageBuffer& outBuffer,
		VmaAllocationInfo* outAllocInfo = nullptr) const;

	void DestroyStageBuffer(const Device::StageBuffer& buffer) const;

	VkResult CreateBuffer(
		VkDeviceSize size,
		VkBufferUsageFlags bufferUsage,
		VmaMemoryUsage memoryUsage,
		VmaAllocationCreateFlags allocFlags,
		Device::Buffer& outBuffer,
		VmaAllocationInfo* outAllocInfo = nullptr) const;

	VkResult CreateBuffer(
		VkDeviceSize size,
		VkBufferUsageFlags bufferUsage,
		VmaMemoryUsage memoryUsage,
		Device::Buffer& outBuffer,
		VmaAllocationInfo* outAllocInfo = nullptr) const;

	VkResult CreateBuffer(
		const VkBufferCreateInfo& bufferCreateInfo,
		const VmaAllocationCreateInfo& allocCreateInfo,
		Device::Buffer& outBuffer,
		VmaAllocationInfo* outAllocInfo = nullptr) const;

	VkResult CreateBufferWithAlignment(
		const VkBufferCreateInfo& bufferCreateInfo,
		const VmaAllocationCreateInfo& allocCreateInfo,
		VkDeviceSize minAlignment,
		Device::Buffer& outBuffer,
		VmaAllocationInfo* outAllocInfo = nullptr) const;

	VkResult CreateBufferWithAlignment(
		VkDeviceSize size,
		VkBufferUsageFlags bufferUsage,
		VmaMemoryUsage memoryUsage,
		VmaAllocationCreateFlags allocFlags,
		VkDeviceSize minAlignment,
		Device::Buffer& outBuffer,
		VmaAllocationInfo* outAllocInfo = nullptr) const;

	void DestroyBuffer(const Device::Buffer& buffer) const;

	// Buffer should have this flag: VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
	[[nodiscard]] VkDeviceAddress GetBufferDeviceAddress(const VkBuffer& buffer) const;

	VkResult CreateAccelerationStructure(AccelerationStructure& outAccelStruct,
		const VkAccelerationStructureCreateInfoKHR& createInfo) const;

	VkResult CreateAccelerationStructure(AccelerationStructure& outAccelStruct,
		const VkAccelerationStructureCreateInfoKHR& createInfo,
		const VmaAllocationCreateInfo&				allocInfo,
		std::span<const uint32_t>                   queueFamilies = {}) const;

	void CreateAccelerationStructure(VkAccelerationStructureTypeKHR type,
		AccelerationStructure&					  outAccelStruct,
		VkAccelerationStructureGeometryKHR&		  geometry,
		VkAccelerationStructureBuildRangeInfoKHR& buildRangeInfo,
		VkBuildAccelerationStructureFlagsKHR	  flags);

	void CreateAccelerationStructure2(VkAccelerationStructureTypeKHR type,
		AccelerationStructure& outAccelStruct,
		std::vector<VkAccelerationStructureGeometryKHR>& geometries,
		std::vector<VkAccelerationStructureBuildRangeInfoKHR>& rangeInfos,
		VkBuildAccelerationStructureFlagsKHR flags);

private:
	void InitSDL();
	void InitInstance();
	void InitSurface();
	void InitDebugMessenger();
	void InitPhysicalDevice();
	void InitLogicalDevice();
	void InitCommands();
	void InitSyncStructures();
	void InitVMA();
	void DestroySDL() const;
	void DestroyInstance() const;
	void DestroySurface() const;
	void DestroyDebugMessenger() const;
	void DestroyLogicalDevice() const;
	void DestroyVMA() const;
	void DestroyCommands() const;
	void DestroySyncStructures() const;

	// Testing
	void InitRayTracing();

	void ProcessSDLEvents();

	bool CheckValidationLayerSupport();
	std::vector<const char*> GetRequiredExtensions();
	void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
	bool IsPhysicalDeviceSuitable(VkPhysicalDevice device);
	bool CheckPhysicalDeviceExtensionSupport(VkPhysicalDevice device);
	SwapchainSupport QuerySwapChainSupport(VkPhysicalDevice device) const;
	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

public:
#if defined (_DEBUG)
	inline static constexpr bool kUseValidationLayers = true;
#else
	inline static constexpr bool kUseValidationLayers = false;
#endif

	static const std::vector<const char*> kValidationLayers;
	static const std::vector<const char*> kPhysDeviceExtensions;

	// Core
	VkInstance					m_instance;
	VkDebugUtilsMessengerEXT	m_debugMessenger;
	VkPhysicalDevice			m_physDevice;
	VkDevice					m_device;
	VkSurfaceKHR				m_surface;
	VkQueue						m_graphicsQueue;
	VkQueue						m_presentQueue;
	u32							m_graphicsQueueFamily;
	QueueFamilies				m_queueFamilies;

	// Immediate / one time submits
	VkFence						m_oneTimeFence;
	VkCommandBuffer				m_oneTimeCommandBuffer;
	VkCommandPool				m_oneTimeCommandPool;

	// Swapchain
	//Swapchain 					m_swapchain;
	bool 						m_windowResized;

	// SDL
	SDL_Window*					m_window = nullptr;
	Input 					    m_input;
	bool						m_quit = false;

	// VMA
	VmaAllocator				m_allocator;

	RuntimeFuncs				m_rtFuncs;

	// Ray tracing test
	//AccelerationStructure              m_tlasAccel;     // Top-level acceleration structure
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_rtProperties;
	VkPhysicalDeviceAccelerationStructurePropertiesKHR m_asProperties;
};

[[nodiscard]] inline VkCommandPoolCreateInfo			CommandPoolCreateInfo(const void* pNext = nullptr);
[[nodiscard]] inline VkCommandBufferAllocateInfo		CommandBufferAllocateInfo(const void* pNext = nullptr);
[[nodiscard]] inline VkCommandBufferBeginInfo			CommandBufferBeginInfo(const void* pNext = nullptr);
[[nodiscard]] inline VkFenceCreateInfo					FenceCreateInfo(const void* pNext = nullptr);
[[nodiscard]] inline VkSemaphoreCreateInfo				SemaphoreCreateInfo(const void* pNext = nullptr);
[[nodiscard]] inline VkSemaphoreSubmitInfo				SemaphoreSubmitInfo(const void* pNext = nullptr);
[[nodiscard]] inline VkCommandBufferSubmitInfo			CommandBufferSubmitInfo(const void* pNext = nullptr);
[[nodiscard]] inline VkSubmitInfo2						SubmitInfo2(const void* pNext = nullptr);
[[nodiscard]] inline VkPresentInfoKHR					PresentInfoKHR(const void* pNext = nullptr);
[[nodiscard]] inline VkRenderingAttachmentInfo			RenderingAttachmentInfo(const void* pNext = nullptr);
[[nodiscard]] inline VkRenderingInfo					RenderingInfo(const void* pNext = nullptr);
[[nodiscard]] inline VkWriteDescriptorSet				WriteDescriptorSet(const void* pNext = nullptr);
[[nodiscard]] inline VkImageCreateInfo					ImageCreateInfo(const void* pNext = nullptr);
[[nodiscard]] inline VkImageViewCreateInfo				ImageViewCreateInfo(const void* pNext = nullptr);
[[nodiscard]] inline VkPipelineLayoutCreateInfo			PipelineLayoutCreateInfo(const void* pNext = nullptr);
[[nodiscard]] inline VkPipelineShaderStageCreateInfo	PipelineShaderStageCreateInfo(const void* pNext = nullptr);
[[nodiscard]] inline VkComputePipelineCreateInfo		ComputePipelineCreateInfo(const void* pNext = nullptr);
[[nodiscard]] inline VkSamplerCreateInfo				SamplerCreateInfo(const void* pNext = nullptr);
[[nodiscard]] inline VkBufferCreateInfo					BufferCreateInfo(const void* pNext = nullptr);

} // namespace trayser

#include <device.inl>
