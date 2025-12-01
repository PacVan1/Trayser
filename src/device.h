#pragma once

#include <SDL.h>
#include <SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <types.h>
#include <vector>

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

struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

class Device
{
public:
	Device();
	void Init();
	void Destroy();
	void ShowCursor(bool show);

private:
	void InitSDL();
	void InitInstance();
	void InitDebugMessenger();
	void InitPhysicalDevice();
	void InitLogicalDevice();
	void InitSurface();
	void InitSwapchain();

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

	VkInstance					m_instance;
	VkDebugUtilsMessengerEXT	m_debugMessenger;
	VkPhysicalDevice			m_physDevice;
	VkDevice					m_device;
	VkSurfaceKHR				m_surface;

	VkSwapchainKHR				m_swapchain;
	VkFormat					m_swapchainImageFormat;
	std::vector<VkImage>		m_swapchainImages;
	std::vector<VkImageView>	m_swapchainImageViews;
	VkExtent2D					m_swapchainExtent;

	SDL_Window*					m_window = nullptr;

	VkQueue						m_graphicsQueue;
	VkQueue						m_presentQueue;

	//VmaAllocator				m_allocator;
};

} // namespace trayser
