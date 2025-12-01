#pragma once

#include <loader.h>
#include <types.h>	
#include <util.h>
#include <descriptors.h>
#include <editor.h>
#include <input.h>
#include <camera.h>
#include <scene.h>
#include <resources.h>
#include <pipelines.h>
#include <device.h>

static constexpr char const*	kEngineName	= "Trayser";
static constexpr unsigned int	kFrameCount	= 2;

#if defined (_DEBUG)
static constexpr bool kUseValidationLayers = true;
#else
static constexpr bool kUseValidationLayers = false;
#endif

static const std::vector<const char*> g_validationLayers	= { "VK_LAYER_KHRONOS_validation" };
static const std::vector<const char*> g_gpuExtensions		= { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

enum PipelineType
{
	PipelineType_Background,
	PipelineType_PBR,
	kPipelineTypeCount,
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

class Engine 
{
public:
	void Init();
	void Cleanup();
	void Render();
	void Run();
	void ImmediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);
	gpu::MeshBuffers UploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);
	AllocatedBuffer CreateBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
	AllocatedImage CreateImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	AllocatedImage CreateImage(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	void DestroyImage(const AllocatedImage& img);
	void DestroyBuffer(const AllocatedBuffer& buffer);
	void HotReloadPipelines();

	[[nodiscard]] FrameData& GetCurrentFrame() { return m_frames[m_frameIdx]; }

private:
	//void InitSwapchain();
	void InitCommands();
	void InitSyncStructures();
	void InitDescriptors();
	void InitPipelines();
	void InitImGui();
	void InitDefaultData();
	void InitDefaultMaterial();
	void CreateSwapchainImageView();
	void DestroySwapchain();
	void BeginRecording(VkCommandBuffer cmd);
	void RenderImGui(VkCommandBuffer cmd, VkImageView targetImageView);
	void ResizeSwapchain();

public:
	Resources					m_resources;
	Editor						m_editor;
	Input						m_input;
	Scene						m_scene;

	Camera						m_camera;

	// Immediate submit structures
	VkFence						m_immFence;
	VkCommandBuffer				m_immCommandBuffer;
	VkCommandPool				m_immCommandPool;

	DescriptorAllocatorGrowable	m_globalDescriptorAllocator;
	VkDescriptorSet				m_renderImageDescriptors;
	VkDescriptorSetLayout		m_renderImageDescriptorLayout;

	AllocatedImage				m_renderImage;
	AllocatedImage				m_depthImage;
	VkExtent2D					m_renderExtent;

	vkutil::DeletionQueue		m_deletionQueue;

	u32							m_graphicsQueueFamily;
	u32							m_frameIdx{0};

	bool						m_isInitialized{false};
	bool						m_stopRendering{false};
	VkExtent2D					m_windowExtent{1700, 900};
	bool						m_resizeRequested = false;

	trayser::Device				m_device;

	RenderMode					m_renderMode = RenderMode_FinalColor;
	trayser::SlangCompiler		m_compiler;
	VkDescriptorSetLayout		m_singleImageDescriptorLayout;

	std::vector<trayser::Pipeline*> m_pipelines;

	VkSampler m_defaultSamplerLinear;
	VkSampler m_defaultSamplerNearest;

	Material m_defaultMaterial;

	// RAY TRACING //////////////////////////////////////////
	//nvvk::DescriptorPack m_rtDescPack;               // Ray tracing descriptor bindings
	//VkPipeline           m_rtPipeline{};             // Ray tracing pipeline
	//VkPipelineLayout     m_rtPipelineLayout{};       // Ray tracing pipeline layout
	//
	//// Acceleration Structure Components
	//std::vector<gpu::AccelerationStructure> m_blasAccel;     // Bottom-level acceleration structures
	//gpu::AccelerationStructure              m_tlasAccel;     // Top-level acceleration structure
	//
	//// Direct SBT management
	//AllocatedBuffer                 sm_sbtBuffer;         // Buffer for shader binding table
	//std::vector<uint8_t>            m_shaderHandles;     // Storage for shader group handles
	//VkStridedDeviceAddressRegionKHR m_raygenRegion{};    // Ray generation shader region
	//VkStridedDeviceAddressRegionKHR m_missRegion{};      // Miss shader region
	//VkStridedDeviceAddressRegionKHR m_hitRegion{};       // Hit shader region
	//VkStridedDeviceAddressRegionKHR m_callableRegion{};  // Callable shader region
	//
	//// Ray Tracing Properties
	//VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_rtProperties{
	//	VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };
	//VkPhysicalDeviceAccelerationStructurePropertiesKHR m_asProperties{
	//	VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR };
	//////////////////////////////////////////////////

private:
	FrameData					m_frames[kFrameCount];

	gpu::SceneData m_sceneData;
	VkDescriptorSetLayout m_gpuSceneDataDescriptorLayout;

	AllocatedImage m_whiteImage;
	AllocatedImage m_blackImage;
	AllocatedImage m_greyImage;


};

extern Engine g_engine;
