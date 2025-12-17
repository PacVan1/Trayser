#include <pch.h>

#include <engine.h>
#include <types.h>
#include <images.h>
#include <pipelines.h>
#include <vk_mem_alloc.h>
#include <stb_image.h>
#include <iostream>

void trayser::Engine::Init()
{
    m_compiler.Init();
    m_device.Init();
    m_gBuffer.Init(kInitWindowWidth, kInitWindowHeight);

    InitDescriptors();
    InitDefaultData();
    //InitTextureDescriptor();
    m_modelPool.Init();
    m_meshPool.Init();
    m_materialPool.Init();
    m_texturePool.Init();

    m_renderer.Init(m_device);
    InitImGuiStyle();

    Model::MikkTSpaceInit();

    ModelHandle handle = m_modelPool.Create(kModelPaths[ModelResource_DamagedHelmet], kModelPaths[ModelResource_DamagedHelmet], this);
    //ModelHandle handle2 = m_modelPool.Create(kModelPaths[ModelResource_Sphere], kModelPaths[ModelResource_Sphere], this);
    const Model& model1 = m_modelPool.Get(handle);
    //const Model& model2 = m_modelPool.Get(handle2);
    m_scene.Init();
    m_scene.CreateModel(model1);
    //m_scene.CreateModel(model2);

    m_scene.Update(1.0f);

    //m_device.CreateBLas2();

    //m_device.CreateTopLevelAs();

    //LoadSkydome(kSkydomePaths[0]);

    InitPipelines();

    InitGpuScene();
}

void trayser::Engine::Destroy()
{
    // Make sure the gpu is done with its work
    vkDeviceWaitIdle(m_device.m_device);

    m_deletionQueue.Flush();

    m_device.Destroy();
}

void trayser::Engine::Render()
{
    VkCommandBuffer cmd = m_renderer.GetCmdBuffer();
    //BeginRecording(cmd);

    //vkutil::TransitionImage(cmd, m_gBuffer.colorImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vkutil::TransitionImage(cmd, m_gBuffer.depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    UpdateGpuScene();

    if (m_rayTraced)
    {
        //m_pipelines[PipelineType_RayTraced]->Update();
        m_rtPipeline.Update();
    }
    else
    {
        m_pipelines[PipelineType_Background]->Update();
        m_pipelines[PipelineType_Rasterized]->Update();
    }

    m_pipelines[PipelineType_Tonemap]->Update();

    // Transition the draw image and the swapchain image into their correct transfer layouts
    vkutil::TransitionImage(cmd, m_gBuffer.colorImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::TransitionImage(cmd, m_renderer.GetSwapchainImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkExtent2D extent = { m_gBuffer.extent.width, m_gBuffer.extent.height };
    vkutil::CopyImageToImage(cmd, m_gBuffer.colorImage.image, m_renderer.GetSwapchainImage(), extent, m_renderer.m_swapchain.extent);

    m_frame++;
}

void trayser::Engine::Run()
{
    while (!m_device.ShouldQuit())
    {
		m_device.NewFrame();
        m_renderer.NewFrame(m_device);

        m_scene.Update(1.0f);
        if (!m_editor.m_update)
            m_camera.Input();
        m_editor.Update();

        Render();
        m_renderer.EndFrame(m_device);
        m_device.EndFrame();
        m_device.EndFrame();

        HotReloadPipelines();
    }
}

void trayser::Engine::InitDescriptors()
{
    // Create a descriptor pool that will hold 10 sets with 1 image each
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes =
    {
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,                 128 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,        128 },
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,    128 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,                128 }
    };

    m_globalDescriptorAllocator.Init(m_device.m_device, 10, sizes);

    // Make the descriptor set layout for our compute draw
    {
    DescriptorLayoutBuilder builder;
    builder.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    m_renderImageDescriptorLayout = builder.Build(m_device.m_device, VK_SHADER_STAGE_COMPUTE_BIT);
    }
    {
    DescriptorLayoutBuilder builder;
    builder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    m_gpuSceneDataDescriptorLayout = builder.Build(m_device.m_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    }
    // Allocate a descriptor set for our draw image
    m_renderImageDescriptors = m_globalDescriptorAllocator.Allocate(m_device.m_device, m_renderImageDescriptorLayout);

    {
    DescriptorWriter writer;
    writer.WriteImage(0, m_gBuffer.colorView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    writer.UpdateSet(m_device.m_device, m_renderImageDescriptors);
    }

    //make sure both the descriptor allocator and the new layout get cleaned up properly
    m_deletionQueue.Push([&]() 
    {
        m_globalDescriptorAllocator.DestroyPools(m_device.m_device);
        vkDestroyDescriptorSetLayout(m_device.m_device, m_renderImageDescriptorLayout, nullptr);
        vkDestroyDescriptorSetLayout(m_device.m_device, m_gpuSceneDataDescriptorLayout, nullptr);
    });
}

void trayser::Engine::InitPipelines()
{
	m_pipelines.push_back(new BackgroundPipeline());
	m_pipelines.push_back(new RasterizedPipeline());
	m_pipelines.push_back(new TonemapPipeline());
	//m_pipelines.push_back(new RayTracedPipeline());

	for (auto& pipeline : m_pipelines)
	{
		pipeline->Init();
	}

    m_rtPipeline.Init();
}

void trayser::Engine::InitImGuiStyle()
{
    ImGuiStyle* style = &ImGui::GetStyle();

    style->WindowPadding = ImVec2(15, 15);
    style->WindowRounding = 5.0f;
    style->FramePadding = ImVec2(5, 5);
    style->FrameRounding = 4.0f;
    style->ItemSpacing = ImVec2(12, 8);
    style->ItemInnerSpacing = ImVec2(8, 6);
    style->IndentSpacing = 25.0f;
    style->ScrollbarSize = 15.0f;
    style->ScrollbarRounding = 9.0f;
    style->GrabMinSize = 5.0f;
    style->GrabRounding = 3.0f;

    style->Colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 0.83f, 1.00f);
    style->Colors[ImGuiCol_TextDisabled] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
    style->Colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
    style->Colors[ImGuiCol_PopupBg] = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
    style->Colors[ImGuiCol_Border] = ImVec4(0.80f, 0.80f, 0.83f, 0.88f);
    style->Colors[ImGuiCol_BorderShadow] = ImVec4(0.92f, 0.91f, 0.88f, 0.00f);
    style->Colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
    style->Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
    style->Colors[ImGuiCol_FrameBgActive] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
    style->Colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
    style->Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.00f, 0.98f, 0.95f, 0.75f);
    style->Colors[ImGuiCol_TitleBgActive] = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
    style->Colors[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
    style->Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
    style->Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
    style->Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
    style->Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
    style->Colors[ImGuiCol_CheckMark] = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
    style->Colors[ImGuiCol_SliderGrab] = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
    style->Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
    style->Colors[ImGuiCol_Button] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
    style->Colors[ImGuiCol_ButtonHovered] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
    style->Colors[ImGuiCol_ButtonActive] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
    style->Colors[ImGuiCol_Header]                  = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
    style->Colors[ImGuiCol_HeaderHovered]           = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
    style->Colors[ImGuiCol_HeaderActive]            = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
    style->Colors[ImGuiCol_ResizeGrip]              = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style->Colors[ImGuiCol_ResizeGripHovered]       = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
    style->Colors[ImGuiCol_ResizeGripActive]        = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
    style->Colors[ImGuiCol_PlotLines]               = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
    style->Colors[ImGuiCol_PlotLinesHovered]        = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
    style->Colors[ImGuiCol_PlotHistogram]           = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
    style->Colors[ImGuiCol_PlotHistogramHovered]    = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
    style->Colors[ImGuiCol_TextSelectedBg]          = ImVec4(0.45f, 0.45f, 0.00f, 0.43f);
    style->Colors[ImGuiCol_SeparatorHovered]        = ImVec4(0.72f, 0.75f, 0.10f, 0.78f);
    style->Colors[ImGuiCol_SeparatorActive]         = ImVec4(0.70f, 0.75f, 0.10f, 1.00f);
    style->Colors[ImGuiCol_Tab]                     = ImVec4(0.37f, 0.08f, 0.08f, 0.86f);
    style->Colors[ImGuiCol_TabHovered]              = ImVec4(0.87f, 0.00f, 0.00f, 0.80f);
    style->Colors[ImGuiCol_TabActive]               = ImVec4(0.87f, 0.00f, 0.00f, 1.00f);
    style->Colors[ImGuiCol_TabUnfocused]            = ImVec4(0.75f, 0.00f, 0.00f, 0.97f);
    style->Colors[ImGuiCol_TabUnfocusedActive]      = ImVec4(0.61f, 0.00f, 0.00f, 1.00f);
}

void trayser::Engine::InitDefaultData()
{
    VkSamplerCreateInfo sampl = SamplerCreateInfo();

    sampl.magFilter = VK_FILTER_LINEAR;
    sampl.minFilter = VK_FILTER_LINEAR;
    vkCreateSampler(m_device.m_device, &sampl, nullptr, &m_sampler);

    VkSamplerCreateInfo samplerCreateInfo = SamplerCreateInfo();
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    vkCreateSampler(m_device.m_device, &samplerCreateInfo, nullptr, &m_samplerCube);
}

void trayser::Engine::InitGpuScene()
{
    m_device.CreateStageBuffer(
        sizeof(gpu::Scene),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        m_sceneBuffer);
    m_sceneBufferAddr = m_device.GetBufferDeviceAddress(m_sceneBuffer.buffer);

    m_device.CreateStageBuffer(
        sizeof(gpu::Mesh) * kMeshCount,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        m_meshBuffer);
    m_meshBufferAddr = m_device.GetBufferDeviceAddress(m_meshBuffer.buffer);

    m_device.CreateStageBuffer(
        sizeof(gpu::Material) * kMaterialCount,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        m_materialBuffer);
    m_materialBufferAddr = m_device.GetBufferDeviceAddress(m_materialBuffer.buffer);

    m_device.CreateStageBuffer(
        sizeof(gpu::Instance) * kInstanceCount,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        m_instanceBuffer);
    m_instanceBufferAddr = m_device.GetBufferDeviceAddress(m_instanceBuffer.buffer);

    m_device.CreateStageBuffer(
        sizeof(gpu::PointLight) * kPointLightCount,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        m_pointLightBuffer);
    m_pointLightBufferAddr = m_device.GetBufferDeviceAddress(m_pointLightBuffer.buffer);
}

void trayser::Engine::UpdateGpuScene()
{
    // Update scene
    gpu::Scene* sceneRef = (gpu::Scene*)m_sceneBuffer.mapped;

    sceneRef->camera.proj = m_camera.m_proj;
    sceneRef->camera.proj[1][1] *= -1.0f;
    sceneRef->camera.view = m_camera.m_view;
    sceneRef->camera.viewProj = sceneRef->camera.proj * sceneRef->camera.view;
    sceneRef->camera.invProj = glm::inverse(sceneRef->camera.proj);
    sceneRef->camera.invView = glm::inverse(sceneRef->camera.view);
    sceneRef->skydomeHandle = m_skydomeHandle;
    sceneRef->lights.pointLightBufferRef = m_pointLightBufferAddr;

    gpu::PointLight* pointLightBufferRef = (gpu::PointLight*)m_pointLightBuffer.mapped;

    for (int i = 0; i < kPointLightCount; i++)
    {
        pointLightBufferRef[i].position  = float3(float(i) - (kPointLightCount / 2), 0.0f, 2.0f);
        pointLightBufferRef[i].intensity = 1.0f;
        pointLightBufferRef[i].color     = float3(1.0f, 1.0f, 1.0f);
    }

    // Update meshes
    gpu::Mesh* meshBufferRef = (gpu::Mesh*)m_meshBuffer.mapped;

    for (int i = 0; i < m_meshPool.m_resources.size(); i++)
    {
        if (!m_meshPool.m_takenSpots[i])
            continue;

        meshBufferRef[i].vertexBufferRef = m_meshPool.m_resources[i].vertexBufferAddr;
        meshBufferRef[i].indexBufferRef = m_meshPool.m_resources[i].indexBufferAddr;
        meshBufferRef[i].primitiveBufferRef = m_meshPool.m_resources[i].primitiveBufferAddr;
    }
    sceneRef->meshBufferRef = m_meshBufferAddr;

    // Update instances
    gpu::Instance* instanceBufferRef = (gpu::Instance*)m_instanceBuffer.mapped;
    gpu::Material* materialBufferRef = (gpu::Material*)m_materialBuffer.mapped;
    
    auto view = g_engine.m_scene.m_registry.view<WorldTransform, RenderComponent>();
    int i = 0;
    for (const auto& [entity, transform, render] : view.each())
    {
        instanceBufferRef[i].transform = transform.matrix;
        instanceBufferRef[i].normalTransform = float4x4(glm::transpose(glm::inverse(float3x3(transform.matrix))));
        instanceBufferRef[i].meshHandle = render.mesh;

        auto& mesh = m_meshPool.m_resources[render.mesh];

        for (auto& prim : mesh.primitives)
        {
            MaterialHandle materialHandle = prim.materialId;
            const Material material = materialHandle != ResourceHandle_Invalid ? m_materialPool.m_resources[materialHandle] : m_renderer.m_defaultMaterial;
            materialBufferRef[materialHandle].baseColorHandle    = material.baseColorHandle != ResourceHandle_Invalid   ? material.baseColorHandle  : m_renderer.m_defaultMaterial.baseColorHandle;
            materialBufferRef[materialHandle].normalMapHandle    = material.normalMapHandle != ResourceHandle_Invalid   ? material.normalMapHandle  : m_renderer.m_defaultMaterial.normalMapHandle;
            materialBufferRef[materialHandle].metalRoughHandle   = material.metalRoughHandle != ResourceHandle_Invalid  ? material.metalRoughHandle : m_renderer.m_defaultMaterial.metalRoughHandle;
            materialBufferRef[materialHandle].aoHandle           = material.aoHandle != ResourceHandle_Invalid          ? material.aoHandle         : m_renderer.m_defaultMaterial.aoHandle;
            materialBufferRef[materialHandle].emissiveHandle     = material.emissiveHandle != ResourceHandle_Invalid    ? material.emissiveHandle   : m_renderer.m_defaultMaterial.emissiveHandle;
            materialBufferRef[materialHandle].emissiveFactor            = material.emissiveFactor;
            materialBufferRef[materialHandle].baseColorFactor           = material.baseColorFactor;
            materialBufferRef[materialHandle].metallicRoughnessAoFactor = material.metallicRoughnessAoFactor;
        }
        i++;
    }
    sceneRef->instanceBufferRef = m_instanceBufferAddr;
    sceneRef->materialBufferRef = m_materialBufferAddr;

    std::vector<VkDescriptorImageInfo> imageInfos;
    std::vector<VkWriteDescriptorSet> writes;
    writes.clear();
    imageInfos.clear();
    imageInfos.reserve(kTextureCount);
    imageInfos.reserve(kTextureCount);
    for (size_t i = 0; i < m_texturePool.m_resources.size(); i++) 
    {
        if (!m_texturePool.m_takenSpots[i])
            continue;

        VkDescriptorImageInfo& imageInfo = imageInfos.emplace_back();
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = m_texturePool.m_resources[i].imageView; // your VkImageView
        imageInfo.sampler = m_sampler;   // your VkSampler

        VkWriteDescriptorSet& write = writes.emplace_back();
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_renderer.GetTextureDescSet();
        write.dstBinding = 0;
        write.dstArrayElement = i;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &imageInfo;
    }

    vkUpdateDescriptorSets(m_device.m_device, writes.size(), writes.data(), 0, nullptr);
}

void trayser::Engine::HotReloadPipelines()
{
    for (int i = 0; i < m_pipelines.size(); i++)
    {
		m_pipelines[i]->ReloadIfChanged();
    }
}

void trayser::Engine::LoadSkydome(const std::string& path)
{
    static constexpr u32 kChannels = 4;

    int width, height;
    float* stbiData = stbi_loadf(path.c_str(), &width, &height, nullptr, kChannels);

    if (!stbiData)
        return;

    m_texturePool.Free(m_skydomeHandle);

    size_t sizeBytes = (size_t)(width * height * kChannels);
    f16* halfedData = new f16[width * height * kChannels];
    for (int i = 0; i < sizeBytes; i++)
    {
        halfedData[i] = stbiData[i];
    }

    m_skydomeHandle = m_texturePool.Create("skydome",
        halfedData,
        (u32)width,
        (u32)height,
        VK_IMAGE_USAGE_SAMPLED_BIT,
        Texture::HDRI{});

    delete[] halfedData;
    stbi_image_free(stbiData);
}

void trayser::Engine::SetAccumulatorDirty()
{
    m_frame = 0;
}

void trayser::Engine::DestroySwapchain()
{
    //vkDestroySwapchainKHR(m_device.m_device, m_device.m_swapchain.m_swapchain, nullptr);
    //
    //for (int i = 0; i < m_device.m_swapchain.m_imageViews.size(); i++)
    //{
    //    vkDestroyImageView(m_device.m_device, m_device.m_swapchain.m_imageViews[i], nullptr);
    //}
}

void trayser::Engine::BeginRecording(VkCommandBuffer cmd)
{
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    VkCommandBufferBeginInfo cmdBeginInfo = CommandBufferBeginInfo();
    cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));
}