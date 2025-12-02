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

namespace trayser
{

class Engine 
{
public:
	void Init();
	void Cleanup();
	void Render();
	void Run();
	gpu::MeshBuffers UploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);
	AllocatedBuffer CreateBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
	AllocatedImage CreateImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	AllocatedImage CreateImage(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	void DestroyImage(const AllocatedImage& img);
	void DestroyBuffer(const AllocatedBuffer& buffer);
	void HotReloadPipelines();

	[[nodiscard]] Input& GetInput()	{ return m_device.m_input; }

private:
	void InitDescriptors();
	void InitPipelines();
	void InitImGuiStyle();
	void InitDefaultData();
	void InitDefaultMaterial();
	void CreateSwapchainImageView();
	void DestroySwapchain();
	void BeginRecording(VkCommandBuffer cmd);
	void ResizeSwapchain();

public:
	Resources		m_resources;
	SlangCompiler	m_compiler;
	Editor			m_editor;
	Scene			m_scene;
	Camera			m_camera;
	Device			m_device;
	RenderMode		m_renderMode = RenderMode_FinalColor;

	std::vector<Pipeline*> m_pipelines;

	DescriptorAllocatorGrowable	m_globalDescriptorAllocator;
	VkDescriptorSet				m_renderImageDescriptors;
	VkDescriptorSetLayout		m_renderImageDescriptorLayout;

	vkutil::DeletionQueue		m_deletionQueue;

	AllocatedImage				m_renderImage;
	AllocatedImage				m_depthImage;
	VkExtent2D					m_renderExtent;

	u32							m_graphicsQueueFamily;
	u32							m_frameIdx{0};

	bool						m_isInitialized{false};
	bool						m_stopRendering{false};
	VkExtent2D					m_windowExtent{1700, 900};
	bool						m_resizeRequested = false;


	VkDescriptorSetLayout		m_singleImageDescriptorLayout;


	VkSampler m_defaultSamplerLinear;
	VkSampler m_defaultSamplerNearest;

	Material m_defaultMaterial;

private:
	gpu::SceneData m_sceneData;
	VkDescriptorSetLayout m_gpuSceneDataDescriptorLayout;

	AllocatedImage m_whiteImage;
	AllocatedImage m_blackImage;
	AllocatedImage m_greyImage;


};

extern Engine g_engine;

} // namespace trayser
