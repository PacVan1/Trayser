#pragma once

#include <SDL.h>
#include <SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>

#include <types.h>
#include <loader.h>
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

// Ray Tracing Properties
static VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_rtProperties{
	VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };
static VkPhysicalDeviceAccelerationStructurePropertiesKHR m_asProperties{
	VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR };

struct Buffer
{
	VkBuffer            buffer;
	VkDeviceAddress	    address;
	VmaAllocation       allocation;
	VmaAllocationInfo   info;
};

struct AccelerationStructure
{
	VkAccelerationStructureKHR  accel{};
	VkDeviceAddress             address{};
	Buffer						buffer;  // Underlying buffer
};

struct QueueFamilyIndices
{
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;

	bool IsComplete() const
	{
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
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

struct SwapChainSupportDetails
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
};

class Swapchain
{
public:
	void Init(VkDevice device);

	FrameData& GetFrame()				{ return m_frames[m_frameIdx]; }
	const FrameData& GetFrame() const	{ return m_frames[m_frameIdx]; }
	VkFence& GetFence()					{ return GetFrame().renderFence; }
	VkImage GetImage() const			{ return m_images[m_imageIdx]; }
	VkCommandBuffer GetCmd() const		{ return GetFrame().commandBuffer; }
	VkImageView GetImageView() const	{ return m_imageViews[m_imageIdx]; }

public:
	VkSwapchainKHR				m_swapchain;
	VkFormat					m_format;
	std::vector<VkImage>		m_images;
	std::vector<VkImageView>	m_imageViews;
	VkExtent2D					m_extent;
	u32 						m_frameIdx;
	u32 						m_imageIdx; // From vkAcquireNextImageKHR
	FrameData					m_frames[kFrameCount];
};

class Device
{
public:
	void Init();
	void Destroy();
	void ShowCursor(bool show);
	void BeginOneTimeSubmit(VkCommandBuffer& outCmd) const;
	void EndOneTimeSubmit() const;
	void BeginFrame();
	void EndFrame();
	bool ShouldQuit() const;
	void CreateBottomLevelAs();
	void CreateTopLevelAs();

	VkCommandBuffer GetCmd() const	{ return m_swapchain.GetCmd(); }
	FrameData& GetFrame() 			{ return m_swapchain.GetFrame(); }

	VkResult CreateBuffer(Buffer& outBuffer,
		VkDeviceSize              size,
		VkBufferUsageFlags2KHR    usage,
		VmaMemoryUsage            memoryUsage	= VMA_MEMORY_USAGE_AUTO,
		VmaAllocationCreateFlags  flags			= {},
		std::span<const uint32_t> queueFamilies = {});

	VkResult CreateBuffer(Buffer& outBuffer,
		VkDeviceSize              size,
		VkBufferUsageFlags2KHR    usage,
		VkDeviceSize              minAlignment,
		VmaMemoryUsage            memoryUsage	= VMA_MEMORY_USAGE_AUTO,
		VmaAllocationCreateFlags  flags			= {},
		std::span<const uint32_t> queueFamilies = {});

	VkResult CreateBuffer(Buffer& outBuffer,
		const VkBufferCreateInfo&	   bufferInfo,
		const VmaAllocationCreateInfo& allocInfo,
		VkDeviceSize                   minAlignment) const;

	VkResult CreateBuffer(Buffer& outBuffer,
		const VkBufferCreateInfo& bufferInfo,
		const VmaAllocationCreateInfo& allocInfo) const;

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

	void PrimitiveToGeometry(const Mesh& mesh,
		VkAccelerationStructureGeometryKHR& geometry,
		VkAccelerationStructureBuildRangeInfoKHR& rangeInfo);

private:
	void InitSDL();
	void InitInstance();
	void InitSurface();
	void InitDebugMessenger();
	void InitPhysicalDevice();
	void InitLogicalDevice();
	void InitSwapchain();
	void InitCommands();
	void InitSyncStructures();
	void InitVMA();
	void InitImGui();

	void RenderImGui() const;
	void ProcessSDLEvents();

	bool CheckValidationLayerSupport();
	std::vector<const char*> GetRequiredExtensions();
	void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
	bool IsPhysicalDeviceSuitable(VkPhysicalDevice device);
	bool CheckPhysicalDeviceExtensionSupport(VkPhysicalDevice device);
	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device) const;
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

	// Immediate / one time submits
	VkFence						m_oneTimeFence;
	VkCommandBuffer				m_oneTimeCommandBuffer;
	VkCommandPool				m_oneTimeCommandPool;

	// Swapchain
	Swapchain 					m_swapchain;
	bool 						m_windowResized;

	// SDL
	SDL_Window*					m_window = nullptr;
	Input 					    m_input;
	bool						m_quit = false;

	// VMA
	VmaAllocator				m_allocator;

	RuntimeFuncs				m_rtFuncs;

	// Ray tracing test
	std::vector<AccelerationStructure> m_blasAccel;     // Bottom-level acceleration structures
	AccelerationStructure              m_tlasAccel;     // Top-level acceleration structure
};

} // namespace trayser
