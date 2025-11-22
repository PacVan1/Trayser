#pragma once

#include <loader.h>
#include <types.h>
#include <util.h>
#include <descriptors.h>
#include <editor.h>
#include <input.h>
#include <camera.h>
#include <scene.h>

constexpr unsigned int kFrameCount{ 2 };

constexpr u32 kModelCount = 2;
constexpr u32 kInitModel = 0;
const std::string kModelPaths[kModelCount] =
{
	"../../assets/basicmesh.glb",
	"../../assets/Khronos/DamagedHelmet/glTF-Binary/DamagedHelmet.glb"
};

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
	DescriptorAllocatorGrowable descriptors;
};

enum RenderMode_ : u8
{
	RenderMode_TexCoord0,
	RenderMode_Tangent,
	RenderMode_Bitangent,
	RenderMode_GeometryNormal,
	RenderMode_ShadingNormal,
	RenderMode_BaseColor,
	RenderMode_NormalMap,
	RenderMode_Metallic,
	RenderMode_Roughness,
	RenderMode_AmbientOcclusion,
	RenderMode_Emission, 
	RenderMode_Wireframe,
	kRenderModeCount
};
using RenderMode = int;

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
	gpu::MeshBuffers UploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

	[[nodiscard]] FrameData& GetCurrentFrame() { return m_frames[m_frameIdx]; }

private:
	void InitVulkan();
	void InitSwapchain();
	void InitCommands();
	void InitSyncStructures();
	void InitDescriptors();
	void InitPipelines();
	void InitBackgroundPipelines();
	void InitTrianglePipelines();
	void InitMeshPipelines();	
	void InitImGui();
	void InitDefaultData();
	void CreateSwapchain(u32 width, u32 height);
	void CreateSwapchainImageView();
	AllocatedBuffer CreateBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
	AllocatedImage CreateImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	AllocatedImage CreateImage(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	void DestroyImage(const AllocatedImage& img);
	void DestroyBuffer(const AllocatedBuffer& buffer);
	void DestroySwapchain();
	void BeginRecording(VkCommandBuffer cmd);
	void RenderBackground(VkCommandBuffer cmd);
	void RenderTriangle(VkCommandBuffer cmd);
	void RenderImGui(VkCommandBuffer cmd, VkImageView targetImageView);
	void ResizeSwapchain();

public:
	Editor						m_editor;
	Input						m_input;
	Scene						m_scene;

	Camera						m_camera;

	// Immediate submit structures
	VkFence						m_immFence;
	VkCommandBuffer				m_immCommandBuffer;
	VkCommandPool				m_immCommandPool;

	VkPipeline					m_triPipeline;
	VkPipelineLayout			m_triPipelineLayout;

	VkPipeline					m_gradientPipeline;
	VkPipelineLayout			m_gradientPipelineLayout;

	DescriptorAllocatorGrowable	m_globalDescriptorAllocator;
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
	bool						m_resizeRequested = false;

	RenderMode					m_renderMode;

private:
	FrameData					m_frames[kFrameCount];
		
	VkPipelineLayout m_meshPipelineLayout;
	VkPipeline m_meshPipeline;

	gpu::MeshBuffers rectangle;
	gpu::SceneData m_sceneData;
	VkDescriptorSetLayout m_gpuSceneDataDescriptorLayout;
	VkDescriptorSetLayout m_singleImageDescriptorLayout;

	AllocatedImage m_whiteImage;
	AllocatedImage m_blackImage;
	AllocatedImage m_greyImage;
	AllocatedImage m_errorCheckerboardImage;

	VkSampler m_defaultSamplerLinear;
	VkSampler m_defaultSamplerNearest;
};
