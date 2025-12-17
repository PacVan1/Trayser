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
#include <../shaders/gpu_io.h>

#include <renderer.h>

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
	void HotReloadPipelines();
	void LoadSkydome(const std::string& path);
	void SetAccumulatorDirty();

	[[nodiscard]] Input& GetInput()	{ return m_device.m_input; }

private:
	void InitDescriptors();
	void InitPipelines();
	void InitImGuiStyle();
	void InitDefaultData();
	void InitGpuScene();
	void InitTextureDescriptor();
	void UpdateGpuScene();
	void DestroySwapchain();
	void BeginRecording(VkCommandBuffer cmd);

public:
	Resources		m_resources;
	SlangCompiler	m_compiler;
	Editor			m_editor;
	Scene			m_scene;
	Camera			m_camera;
	Device			m_device;
	GBuffer			m_gBuffer;
	Renderer		m_renderer;

	RenderMode		m_renderMode	= RenderMode_FinalColor;
	TonemapMode		m_tonemapMode	= TonemapMode_ACES;
	PipelineMode	m_pipelineMode	= PipelineMode_Rasterized;

	static constexpr uint32_t kMeshCount		= 128;
	static constexpr uint32_t kInstanceCount	= 128;
	static constexpr uint32_t kMaterialCount	= 128;

	ResourcePool<Model, 10> m_modelPool;
	ResourcePool<Mesh, kMeshCount> m_meshPool;
	ResourcePool<Texture, kTextureCount> m_texturePool;
	ResourcePool<Material, kMaterialCount> m_materialPool;

	bool			m_rayTraced = true;
	uint32_t		m_frame = 0;

	TextureHandle m_skydomeHandle;

	Device::Buffer			m_gpuScene;
	Device::Buffer			m_meshBuffer;
	Device::Buffer			m_instanceBuffer;
	Device::Buffer			m_materialBuffer;
	Device::Buffer			m_pointLightBuffer;
	Device::Buffer			m_dirLightBuffer;
	VkDeviceAddress			m_gpuSceneAddr;
	VkDeviceAddress			m_meshBufferAddr;
	VkDeviceAddress			m_instanceBufferAddr;
	VkDeviceAddress			m_materialBufferAddr;
	VkDeviceAddress			m_pointLightBufferAddr;
	VkDeviceAddress			m_dirLightBufferAddr;
	gpu::Scene*				m_sceneMapped;
	gpu::Mesh*				m_meshMapped;
	gpu::Instance*			m_instanceMapped;
	gpu::Material*			m_materialMapped;
	gpu::PointLight*		m_pointLightMapped;
	gpu::DirectionalLight*	m_dirLightMapped;

	std::vector<Pipeline*>	m_pipelines;
	RayTracedPipeline		m_rtPipeline;

	DescriptorAllocatorGrowable	m_globalDescriptorAllocator;
	VkDescriptorSet				m_renderImageDescriptors;
	VkDescriptorSetLayout		m_renderImageDescriptorLayout;

	vkutil::DeletionQueue		m_deletionQueue;
	u32							m_graphicsQueueFamily;

	VkExtent2D					m_windowExtent{1700, 900};
	VkSampler m_sampler;
	VkSampler m_samplerCube;

	//VkDescriptorSetLayout m_allTexturesLayout;
	//VkDescriptorSet m_allTexturesSet;

private:
	VkDescriptorSetLayout m_gpuSceneDataDescriptorLayout;
};

inline Engine g_engine;

} // namespace trayser
