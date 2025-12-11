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
#include <gbuffer.h>
#include <slang_compiler.h>
#include <gpu_io.h>

static constexpr char const*	kEngineName	= "Trayser";
static constexpr unsigned int	kFrameCount	= 2;

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
	void Destroy();
	void Render();
	void Run();
	gpu::MeshBuffers UploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);
	AllocatedBuffer CreateBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
	AllocatedImage CreateImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	AllocatedImage CreateImage(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	void DestroyImage(const AllocatedImage& img);
	void DestroyBuffer(const AllocatedBuffer& buffer);
	void HotReloadPipelines();
	void LoadSkydome(const std::string& path);
	void SetAccumulatorDirty();

	[[nodiscard]] Input& GetInput()	{ return m_device.m_input; }

private:
	void InitDescriptors();
	void InitPipelines();
	void InitImGuiStyle();
	void InitDefaultData();
	void InitDefaultMaterial();
	void InitGpuScene();
	void InitTextureDescriptor();
	void UpdateGpuScene();
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
	GBuffer			m_gBuffer;

	RenderMode		m_renderMode	= RenderMode_Depth;
	TonemapMode		m_tonemapMode	= TonemapMode_ACES;
	PipelineMode	m_pipelineMode	= PipelineMode_Rasterized;

	static constexpr uint32_t kMeshCount		= 128;
	static constexpr uint32_t kInstanceCount	= 128;
	static constexpr uint32_t kTextureCount		= 128;
	static constexpr uint32_t kMaterialCount	= 128;

	ResourcePool<Model, 10> m_modelPool;
	ResourcePool<Mesh, kMeshCount> m_meshPool;
	ResourcePool<Image, kTextureCount> m_texturePool;
	ResourcePool<Material2, kMaterialCount> m_materialPool;

	bool			m_rayTraced = true;
	uint32_t		m_frame = 0;

	TextureHandle m_skydomeHandle;

	AllocatedBuffer m_gpuScene;
	VkDeviceAddress m_gpuSceneAddr;
	AllocatedBuffer m_meshBuffer;
	VkDeviceAddress m_meshBufferAddr;
	AllocatedBuffer m_instanceBuffer;
	VkDeviceAddress m_instanceBufferAddr;
	AllocatedBuffer m_materialBuffer;
	VkDeviceAddress m_materialBufferAddr;

	std::vector<Pipeline*> m_pipelines;

	DescriptorAllocatorGrowable	m_globalDescriptorAllocator;
	VkDescriptorSet				m_renderImageDescriptors;
	VkDescriptorSetLayout		m_renderImageDescriptorLayout;

	vkutil::DeletionQueue		m_deletionQueue;
	u32							m_graphicsQueueFamily;

	VkExtent2D					m_windowExtent{1700, 900};
	VkSampler m_sampler;
	VkSampler m_samplerCube;
	Material m_defaultMaterial;

	VkDescriptorSetLayout m_allTexturesLayout;
	VkDescriptorSet m_allTexturesSet;

private:
	gpu::SceneData m_sceneData;
	VkDescriptorSetLayout m_gpuSceneDataDescriptorLayout;
};

inline Engine g_engine;

} // namespace trayser
