#include <pch.h>
#include <pipelines.h>
#include <fstream>
//#include <initializers.h>
#include <engine.h>
#include <compiler.h>
#include <filesystem>
#include <array>

void trayser::Pipeline::Init()
{
    if (m_canHotReload)
    {
        std::string filePath = g_engine.m_compiler.FindExistingFile((m_name + ".slang").c_str());
        if (filePath.empty())
            return;

        m_lastWriteTime = std::filesystem::last_write_time(filePath);
    }

    Load();
}

void trayser::Pipeline::Destroy() const
{
	vkDestroyPipelineLayout(g_engine.m_device.m_device, m_layout, nullptr);
	vkDestroyPipeline(g_engine.m_device.m_device, m_pipeline, nullptr);
}

void trayser::Pipeline::ReloadIfChanged()
{
	if (!m_canHotReload)
		return;

    std::string filePath = g_engine.m_compiler.FindExistingFile((m_name + ".slang").c_str());
	if (filePath.empty())
		return;

    auto currentLastWriteTime = std::filesystem::last_write_time(filePath);
    // Shader source has changed
    if (m_lastWriteTime < currentLastWriteTime)
    {   
        m_lastWriteTime = currentLastWriteTime;

        vkQueueWaitIdle(g_engine.m_device.m_graphicsQueue);
        Load();
    }
}

trayser::PBRPipeline::PBRPipeline()
{
	m_name = "pbr";
	m_canHotReload = true;
}

void trayser::PBRPipeline::Load()
{
    std::array<VkShaderModule, 2> modules;
    const char* entryPointNames[] = { "vsMain", "fsMain" };

    SlangCompileInfo compileInfo{};
    compileInfo.fileName        = m_name.c_str();
    compileInfo.entryPointNames = entryPointNames;
    compileInfo.entryPointCount = 2;
	g_engine.m_compiler.Compile(compileInfo, modules.data());

	//if (!spirv[0] || !spirv[1])
	//{
	//	fmt::println("Failed to load PBR shaders");
	//	return;
	//}

    VkPushConstantRange pushConst{};
    pushConst.offset = 0;
    pushConst.size = sizeof(gpu::PushConstants);
    pushConst.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkPipelineLayoutCreateInfo pipLayoutInfo = PipelineLayoutCreateInfo();
    pipLayoutInfo.flags = 0;
    pipLayoutInfo.setLayoutCount = 1;
    pipLayoutInfo.pSetLayouts = &g_engine.m_singleImageDescriptorLayout;
    pipLayoutInfo.pushConstantRangeCount = 1;
    pipLayoutInfo.pPushConstantRanges = &pushConst;
    VK_CHECK(vkCreatePipelineLayout(g_engine.m_device.m_device, &pipLayoutInfo, nullptr, &m_layout));

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    auto bindingDescription = Vertex::GetBindingDescription();
    auto attributeDescriptions = Vertex::GetAttributeDescriptions();
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &bindingDescription; // Optional
    vertexInput.vertexAttributeDescriptionCount = attributeDescriptions.size();
    vertexInput.pVertexAttributeDescriptions = attributeDescriptions.data(); // Optional

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineRasterizationStateCreateInfo rasterization{};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterization.lineWidth = 1.0f;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.sampleShadingEnable = VK_FALSE;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisample.minSampleShading = 1.0f;
    multisample.pSampleMask = nullptr;
    multisample.alphaToCoverageEnable = VK_FALSE;
    multisample.alphaToOneEnable = VK_FALSE;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;
    depthStencil.front = {};
    depthStencil.back = {};
    depthStencil.minDepthBounds = 0.f;
    depthStencil.maxDepthBounds = 1.f;

    VkPipelineRenderingCreateInfo rendering{};
    rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachmentFormats = &g_engine.m_gBuffer.colorImage.imageFormat;
    rendering.depthAttachmentFormat = g_engine.m_gBuffer.depthImage.imageFormat;

    VkPipelineShaderStageCreateInfo vsStage{};
    vsStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vsStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vsStage.module = modules[0];
    vsStage.pName = "main";

    VkPipelineShaderStageCreateInfo fsStage{};
    fsStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fsStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fsStage.module = modules[1];
    fsStage.pName = "main";

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.pNext = nullptr;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.pNext = nullptr;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkDynamicState dynamicStates[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo{};
    dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateCreateInfo.pDynamicStates = dynamicStates;
    dynamicStateCreateInfo.dynamicStateCount = 2;

    VkPipelineShaderStageCreateInfo shaderStages[2] = { vsStage, fsStage };

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &rendering;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterization;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pDynamicState = &dynamicStateCreateInfo;
    pipelineInfo.layout = m_layout;

    if (vkCreateGraphicsPipelines(g_engine.m_device.m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS)
    {
        fmt::println("Failed to create pipeline");
        return;
    }

    vkDestroyShaderModule(g_engine.m_device.m_device, modules[0], nullptr);
    vkDestroyShaderModule(g_engine.m_device.m_device, modules[1], nullptr);
}

void trayser::PBRPipeline::Update()
{
    auto cmd = g_engine.m_device.GetCmd();

    VkExtent2D extent = { g_engine.m_gBuffer.colorImage.imageExtent.width, g_engine.m_gBuffer.colorImage.imageExtent.height };

    VkRenderingAttachmentInfo colorAttachment = RenderingAttachmentInfo();
    colorAttachment.imageView   = g_engine.m_gBuffer.colorImage.imageView;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp      = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingAttachmentInfo depthAttachment = RenderingAttachmentInfo();
    depthAttachment.imageView   = g_engine.m_gBuffer.depthImage.imageView;
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.clearValue.depthStencil.depth = 0.f;

    VkRenderingInfo renderingInfo = RenderingInfo();
    renderingInfo.renderArea = VkRect2D{ VkOffset2D { 0, 0 }, extent };
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = &depthAttachment;
    renderingInfo.pStencilAttachment = nullptr;
    vkCmdBeginRendering(cmd, &renderingInfo);

    //set dynamic viewport and scissor
    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = extent.width;
    viewport.height = extent.height;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;

    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = extent.width;
    scissor.extent.height = extent.height;

    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

    auto view = g_engine.m_scene.m_registry.view<WorldTransform, RenderComponent>();
    for (const auto& [ent, tf, render] : view.each())
    {
        for (auto& prim : render.mesh->primitives)
        {
            //bind a texture
            VkDescriptorSet imageSet = g_engine.m_device.GetFrame().descriptors.Allocate(g_engine.m_device.m_device, g_engine.m_singleImageDescriptorLayout);
            {
                DescriptorWriter writer;
                writer.WriteImage(0, render.mesh->materials[prim.materialId].baseColor ? render.mesh->materials[prim.materialId].baseColor->imageView : g_engine.m_defaultMaterial.baseColor->imageView, g_engine.m_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                writer.WriteImage(1, render.mesh->materials[prim.materialId].normalMap ? render.mesh->materials[prim.materialId].normalMap->imageView : g_engine.m_defaultMaterial.normalMap->imageView, g_engine.m_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                writer.WriteImage(2, render.mesh->materials[prim.materialId].metallicRoughness ? render.mesh->materials[prim.materialId].metallicRoughness->imageView : g_engine.m_defaultMaterial.metallicRoughness->imageView, g_engine.m_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                writer.WriteImage(3, render.mesh->materials[prim.materialId].occlusion ? render.mesh->materials[prim.materialId].occlusion->imageView : g_engine.m_defaultMaterial.occlusion->imageView, g_engine.m_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                writer.WriteImage(4, render.mesh->materials[prim.materialId].emissive ? render.mesh->materials[prim.materialId].emissive->imageView : g_engine.m_defaultMaterial.emissive->imageView, g_engine.m_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

                writer.UpdateSet(g_engine.m_device.m_device, imageSet);
            }

            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_layout, 0, 1, &imageSet, 0, nullptr);

            gpu::PushConstants pushConst{};
            glm::mat4 projection = g_engine.m_camera.m_proj;
            projection[1][1] *= -1;
            pushConst.renderMode = glm::ivec4(g_engine.m_renderMode, 0, 0, 0);
            pushConst.camPos = glm::vec4(g_engine.m_camera.m_transform.translation, 1.0f);

            pushConst.viewProj = projection * g_engine.m_camera.m_view;
            pushConst.model = tf.matrix;

            VkBuffer vertexBuffers[] = { render.mesh->vertexBuffer.buffer };
            VkDeviceSize offsets[] = { 0 };
            vkCmdBindIndexBuffer(cmd, render.mesh->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);

            vkCmdPushConstants(cmd, m_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(gpu::PushConstants), &pushConst);
            //vkCmdBindIndexBuffer(cmd, render.mesh->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

            vkCmdDrawIndexed(cmd, render.mesh->indexCount, 1, 0, 0, 0);
            //vkCmdDraw(cmd, prim.vertexCount, 1, 0, 0);
        }
    }

    vkCmdEndRendering(cmd);
}

trayser::BackgroundPipeline::BackgroundPipeline()
{
	m_name = "sky.comp";
	m_canHotReload = false;
}

void trayser::BackgroundPipeline::Load()
{
    VkPipelineLayoutCreateInfo computeLayout{};
    computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    computeLayout.pNext = nullptr;
    computeLayout.pSetLayouts = &g_engine.m_renderImageDescriptorLayout;
    computeLayout.setLayoutCount = 1;

    VkPushConstantRange pushConstant{};
    pushConstant.offset = 0;
    pushConstant.size = sizeof(ComputePushConstants);
    pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    computeLayout.pPushConstantRanges = &pushConstant;
    computeLayout.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(g_engine.m_device.m_device, &computeLayout, nullptr, &m_layout));

    // Layout code
    VkShaderModule computeDrawShader;
    if (!g_engine.m_compiler.LoadShaderModule(m_name.c_str(), computeDrawShader))
    {
        fmt::print("Error when building the compute shader \n");
    }

    VkPipelineShaderStageCreateInfo stageinfo{};
    stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageinfo.pNext = nullptr;
    stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageinfo.module = computeDrawShader;
    stageinfo.pName = "main";

    VkComputePipelineCreateInfo computePipelineCreateInfo{};
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.pNext = nullptr;
    computePipelineCreateInfo.layout = m_layout;
    computePipelineCreateInfo.stage = stageinfo;

    VK_CHECK(vkCreateComputePipelines(g_engine.m_device.m_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &m_pipeline));

    vkDestroyShaderModule(g_engine.m_device.m_device, computeDrawShader, nullptr);
}

void trayser::BackgroundPipeline::Update()
{
    auto cmd = g_engine.m_device.GetCmd();

    // bind the gradient drawing compute pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);

    // bind the descriptor set containing the draw image for the compute pipeline
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_layout, 0, 1, &g_engine.m_renderImageDescriptors, 0, nullptr);

    ComputePushConstants pc;
    pc.data1 = glm::vec4(0.1, 0.2, 0.4, 0.97);

    vkCmdPushConstants(cmd, m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &pc);

    VkExtent2D extent = { g_engine.m_gBuffer.colorImage.imageExtent.width, g_engine.m_gBuffer.colorImage.imageExtent.height };

    // execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
    vkCmdDispatch(cmd, std::ceil(extent.width / 16.0), std::ceil(extent.height / 16.0), 1);
}

trayser::TonemapPipeline::TonemapPipeline() :
	m_descriptorSetLayout(VK_NULL_HANDLE),
	m_descriptorSet(VK_NULL_HANDLE)
{
    m_name = "tonemap";
    m_canHotReload = true;
}

void trayser::TonemapPipeline::Load()
{
    // Descriptor set layout

    DescriptorLayoutBuilder builder;
    builder.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    builder.AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    m_descriptorSetLayout = builder.Build(g_engine.m_device.m_device, VK_SHADER_STAGE_COMPUTE_BIT);

    m_descriptorSet = g_engine.m_globalDescriptorAllocator.Allocate(g_engine.m_device.m_device, m_descriptorSetLayout);

    DescriptorWriter writer;
    writer.WriteImage(0, g_engine.m_gBuffer.colorImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    writer.WriteImage(1, g_engine.m_gBuffer.colorImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    writer.UpdateSet(g_engine.m_device.m_device, m_descriptorSet);

    // ---------------------

    VkPipelineLayoutCreateInfo computeLayout{};
    computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    computeLayout.pNext = nullptr;
    computeLayout.pSetLayouts = &m_descriptorSetLayout;
    computeLayout.setLayoutCount = 1;

    VkPushConstantRange pushConstant{};
    pushConstant.offset = 0;
    pushConstant.size = sizeof(PushConstantComp);
    pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    computeLayout.pPushConstantRanges = &pushConstant;
    computeLayout.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(g_engine.m_device.m_device, &computeLayout, nullptr, &m_layout));

    std::array<VkShaderModule, 1> modules;
    const char* entryPointNames[] = { "csMain" };

    SlangCompileInfo compileInfo{};
    compileInfo.fileName        = m_name.c_str();
    compileInfo.entryPointNames = entryPointNames;
    compileInfo.entryPointCount = 1;
    g_engine.m_compiler.Compile(compileInfo, modules.data());

    VkPipelineShaderStageCreateInfo stageinfo{};
    stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageinfo.pNext = nullptr;
    stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageinfo.module = modules[0];
    stageinfo.pName = "main";

    VkComputePipelineCreateInfo computePipelineCreateInfo{};
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.pNext = nullptr;
    computePipelineCreateInfo.layout = m_layout;
    computePipelineCreateInfo.stage = stageinfo;

    VK_CHECK(vkCreateComputePipelines(g_engine.m_device.m_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &m_pipeline));

    vkDestroyShaderModule(g_engine.m_device.m_device, modules[0], nullptr);
}

void trayser::TonemapPipeline::Update()
{
    auto cmd = g_engine.m_device.GetCmd();

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_layout, 0, 1, &m_descriptorSet, 0, nullptr);

    PushConstantComp pc{};
    pc.tonemapMode = g_engine.m_tonemapMode;

    vkCmdPushConstants(cmd, m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantComp), &pc);

    VkExtent2D extent = { g_engine.m_gBuffer.colorImage.imageExtent.width, g_engine.m_gBuffer.colorImage.imageExtent.height };

    // execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
    vkCmdDispatch(cmd, std::ceil(extent.width / 16.0), std::ceil(extent.height / 16.0), 1);
}

trayser::RayTracingPipeline::RayTracingPipeline() :
    m_descriptorSetLayout(VK_NULL_HANDLE),
    m_descriptorSet(VK_NULL_HANDLE)
{
    m_name = "ray_tracing";
    m_canHotReload = true;
}

void trayser::RayTracingPipeline::Load()
{
    // Descriptor set layout
    
    DescriptorLayoutBuilder builder;
    builder.AddBinding(BindingPoints_TLAS, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
    builder.AddBinding(BindingPoints_OutImage, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    m_descriptorSetLayout = builder.Build(g_engine.m_device.m_device, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
    
    m_descriptorSet = g_engine.m_globalDescriptorAllocator.Allocate(g_engine.m_device.m_device, m_descriptorSetLayout);

    DescriptorWriter writer;
    writer.WriteImage(1, g_engine.m_gBuffer.colorImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    writer.UpdateSet(g_engine.m_device.m_device, m_descriptorSet);

    for (auto& b : builder.m_bindings) {
        printf("binding=%u type=%u count=%u stageFlags=0x%x\n",
            b.binding, b.descriptorType, b.descriptorCount, b.stageFlags);
    }
    
    // ---------------------
    
    // For re-creation
    //vkDestroyPipeline(m_app->getDevice(), m_rtPipeline, nullptr);
    //vkDestroyPipelineLayout(m_app->getDevice(), m_rtPipelineLayout, nullptr);
    
    // Creating all shaders
    enum StageIndices
    {
        eRaygen,
        eMiss,
        eClosestHit,
        eShaderGroupCount
    };
    std::array<VkPipelineShaderStageCreateInfo, eShaderGroupCount> stages{};
    for (auto& s : stages)
        s.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    
    // Compile shader, fallback to pre-compiled
    VkShaderModule shaderModules[3];
    const char* entryPointNames[3] = { "rgMain", "mMain", "chMain" };
    SlangCompileInfo compileInfo{};
    compileInfo.entryPointCount = 3;
    compileInfo.entryPointNames = entryPointNames;
    compileInfo.fileName        = m_name.c_str();

    g_engine.m_compiler.Compile(compileInfo, shaderModules);
    
    stages[eRaygen].pNext = nullptr;
    stages[eRaygen].pName = "main";
    stages[eRaygen].stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    stages[eRaygen].module = shaderModules[eRaygen];
    stages[eMiss].pNext = nullptr;
    stages[eMiss].pName = "main";
    stages[eMiss].stage = VK_SHADER_STAGE_MISS_BIT_KHR;
    stages[eMiss].module = shaderModules[eMiss];
    stages[eClosestHit].pNext = nullptr;
    stages[eClosestHit].pName = "main";
    stages[eClosestHit].stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    stages[eClosestHit].module = shaderModules[eClosestHit];
    
    // Shader groups
    VkRayTracingShaderGroupCreateInfoKHR group{ VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR };
    group.anyHitShader = VK_SHADER_UNUSED_KHR;
    group.closestHitShader = VK_SHADER_UNUSED_KHR;
    group.generalShader = VK_SHADER_UNUSED_KHR;
    group.intersectionShader = VK_SHADER_UNUSED_KHR;
    
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> shader_groups;
    // Raygen
    group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    group.generalShader = eRaygen;
    shader_groups.push_back(group);
    
    // Miss
    group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    group.generalShader = eMiss;
    shader_groups.push_back(group);
    
    // closest hit shader
    group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    group.generalShader = VK_SHADER_UNUSED_KHR;
    group.closestHitShader = eClosestHit;
    shader_groups.push_back(group);
    
    // Push constant: we want to be able to update constants used by the shaders
    const VkPushConstantRange push_constant{ VK_SHADER_STAGE_ALL, 0, sizeof(PushConstants) };
    
    VkPipelineLayoutCreateInfo pipeline_layout_create_info{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipeline_layout_create_info.pushConstantRangeCount = 1;
    pipeline_layout_create_info.pPushConstantRanges = &push_constant;
    
    // Descriptor sets: one specific to ray tracing, and one shared with the rasterization pipeline
    pipeline_layout_create_info.setLayoutCount = 1;
    pipeline_layout_create_info.pSetLayouts = &m_descriptorSetLayout;
    VK_CHECK(vkCreatePipelineLayout(g_engine.m_device.m_device, &pipeline_layout_create_info, nullptr, &m_layout));
    //NVVK_DBG_NAME(m_rtPipelineLayout);
    
    // Assemble the shader stages and recursion depth info into the ray tracing pipeline
    VkRayTracingPipelineCreateInfoKHR rtPipelineInfo{ VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR };
    rtPipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
    rtPipelineInfo.pStages = stages.data();
    rtPipelineInfo.groupCount = static_cast<uint32_t>(shader_groups.size());
    rtPipelineInfo.pGroups = shader_groups.data();
    rtPipelineInfo.maxPipelineRayRecursionDepth = std::max(3U, g_engine.m_device.m_rtProperties.maxRayRecursionDepth);
    rtPipelineInfo.layout = m_layout;
    VK_CHECK(g_engine.m_device.m_rtFuncs.vkCreateRayTracingPipelinesKHR(g_engine.m_device.m_device, {}, {}, 1, &rtPipelineInfo, nullptr, &m_pipeline));
    //NVVK_DBG_NAME(m_rtPipeline);
    
    //LOGI("Ray tracing pipeline created successfully\n");
    
    // Shader binding table
    
    //m_allocator.destroyBuffer(m_sbtBuffer);  // Cleanup when re-creating
    
    VkDevice device = g_engine.m_device.m_device;
    uint32_t handleSize = g_engine.m_device.m_rtProperties.shaderGroupHandleSize;
    uint32_t handleAlignment = g_engine.m_device.m_rtProperties.shaderGroupHandleAlignment;
    uint32_t baseAlignment = g_engine.m_device.m_rtProperties.shaderGroupBaseAlignment;
    uint32_t groupCount = rtPipelineInfo.groupCount;
    
    // Get shader group handles
    size_t dataSize = handleSize * groupCount;
    m_shaderHandles.resize(dataSize);
    VK_CHECK(g_engine.m_device.m_rtFuncs.vkGetRayTracingShaderGroupHandlesKHR(device, m_pipeline, 0, groupCount, dataSize, m_shaderHandles.data()));
    
    // Calculate SBT buffer size with proper alignment
    auto     alignUp = [](uint32_t size, uint32_t alignment) { return (size + alignment - 1) & ~(alignment - 1); };
    uint32_t raygenSize = alignUp(handleSize, handleAlignment);
    uint32_t missSize = alignUp(handleSize, handleAlignment);
    uint32_t hitSize = alignUp(handleSize, handleAlignment);
    uint32_t callableSize = 0;  // No callable shaders in this tutorial
    
    // Ensure each region starts at a baseAlignment boundary
    uint32_t raygenOffset = 0;
    uint32_t missOffset = alignUp(raygenSize, baseAlignment);
    uint32_t hitOffset = alignUp(missOffset + missSize, baseAlignment);
    uint32_t callableOffset = alignUp(hitOffset + hitSize, baseAlignment);
    
    size_t bufferSize = callableOffset + callableSize;
    
    // Create SBT buffer
    VK_CHECK(g_engine.m_device.CreateBuffer(m_sbtBuffer, bufferSize, VK_BUFFER_USAGE_2_SHADER_BINDING_TABLE_BIT_KHR, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT));
    //NVVK_DBG_NAME(m_sbtBuffer.buffer);
    
    // Populate SBT buffer
    uint8_t* pData = static_cast<uint8_t*>(m_sbtBuffer.info.pMappedData);
    
    // Ray generation shader (group 0)
    memcpy(pData + raygenOffset, m_shaderHandles.data() + 0 * handleSize, handleSize);
    m_raygenRegion.deviceAddress = m_sbtBuffer.address + raygenOffset;
    m_raygenRegion.stride = raygenSize;
    m_raygenRegion.size = raygenSize;
    
    // Miss shader (group 1)
    memcpy(pData + missOffset, m_shaderHandles.data() + 1 * handleSize, handleSize);
    m_missRegion.deviceAddress = m_sbtBuffer.address + missOffset;
    m_missRegion.stride = missSize;
    m_missRegion.size = missSize;
    
    // Hit shader (group 2)
    memcpy(pData + hitOffset, m_shaderHandles.data() + 2 * handleSize, handleSize);
    m_hitRegion.deviceAddress = m_sbtBuffer.address + hitOffset;
    m_hitRegion.stride = hitSize;
    m_hitRegion.size = hitSize;
    
    // Callable shaders (none in this tutorial)
    m_callableRegion.deviceAddress = 0;
    m_callableRegion.stride = 0;
    m_callableRegion.size = 0;
    
    ///////////////////////
}

void trayser::RayTracingPipeline::Update()
{
    // Ray trace pipeline
    vkCmdBindPipeline(g_engine.m_device.GetCmd(), VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_pipeline);

    // Bind the descriptor sets for the graphics pipeline (making textures available to the shaders)
    //const VkBindDescriptorSetsInfo bindDescriptorSetsInfo{ .sType = VK_STRUCTURE_TYPE_BIND_DESCRIPTOR_SETS_INFO,
    //                                                      .stageFlags = VK_SHADER_STAGE_ALL,
    //                                                      .layout = m_layout,
    //                                                      .firstSet = 0,
    //                                                      .descriptorSetCount = 1,
    //                                                      .pDescriptorSets = m_descPack.getSetPtr() };
    //vkCmdBindDescriptorSets2(cmd, &bindDescriptorSetsInfo);
    
    // Push descriptor sets for ray tracing
    //nvvk::WriteSetContainer write{};
    //write.append(m_rtDescPack.makeWrite(shaderio::BindingPoints::eTlas), g_engine.m_device.m_tlasAccel);
    //write.append(m_rtDescPack.makeWrite(shaderio::BindingPoints::eOutImage), g_engine.m_gBuffer.colorImage),
    //    VK_IMAGE_LAYOUT_GENERAL);
    //vkCmdPushDescriptorSetKHR(g_engine.m_device.GetCmd(), VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_layout, 1, write.size(), write.data());

    VkAccelerationStructureDeviceAddressInfoKHR ai{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
    ai.accelerationStructure = g_engine.m_device.m_tlasAccel.accel;
    VkDeviceAddress addr = g_engine.m_device.m_rtFuncs.vkGetAccelerationStructureDeviceAddressKHR(g_engine.m_device.m_device, &ai);
    assert(addr != 0); // 0 means invalid/never built

    {
        DescriptorWriter writer;
        writer.WriteAccelStruct(0, g_engine.m_device.m_tlasAccel.accel);
        writer.UpdateSet(g_engine.m_device.m_device, m_descriptorSet);
    }

    vkCmdBindDescriptorSets(g_engine.m_device.GetCmd(), VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_layout, 0, 1, &m_descriptorSet, 0, nullptr);

    // Push constant information
    PushConstants pushValues{};
    pushValues.projInvMatrix = glm::inverse(g_engine.m_camera.m_proj);
    pushValues.viewInvMatrix = glm::inverse(g_engine.m_camera.m_view);

    vkCmdPushConstants(g_engine.m_device.GetCmd(), 
        m_layout,
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 
        0, 
        sizeof(PushConstants),
        &pushValues);

    // Ray trace
    const VkExtent2D size = { kInitWindowWidth, kInitWindowHeight };
    g_engine.m_device.m_rtFuncs.vkCmdTraceRaysKHR(g_engine.m_device.GetCmd(), &m_raygenRegion, &m_missRegion, &m_hitRegion, &m_callableRegion, size.width, size.height, 1);

    // Barrier to make sure the image is ready for Tonemapping
    //nvvk::cmdMemoryBarrier(g_engine.m_device.GetCmd(), VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
}
