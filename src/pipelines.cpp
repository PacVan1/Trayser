#include <pch.h>
#include <pipelines.h>
#include <fstream>
#include <initializers.h>
#include <engine.h>
#include <filesystem>

static void Diagnose(Slang::ComPtr<slang::IBlob> blob)
{
    if (blob != nullptr)
    {
        printf("%s", (const char*)blob->getBufferPointer());
    }
}

VkShaderModule CreateShaderModule(VkDevice device, const Slang::ComPtr<slang::IBlob>& blob)
{
    VkShaderModuleCreateInfo ci{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    ci.codeSize = blob->getBufferSize();
    ci.pCode = reinterpret_cast<const uint32_t*>(blob->getBufferPointer());

    VkShaderModule module = VK_NULL_HANDLE;
    VkResult vr = vkCreateShaderModule(device, &ci, nullptr, &module);
    if (vr != VK_SUCCESS) { printf("Failed to create shader module"); }
    return module;
}

void trayser::SlangCompiler::Init()
{
    SlangResult result{};
    result = slang::createGlobalSession(m_slangGlobalSession.writeRef());
    if (SLANG_FAILED(result))
    {
        fmt::println("Failed to create Slang global session");
        return;
    }

    m_searchPaths =
    {
        "shaders"
        "../shaders",
        "../../shaders",
        "../../../shaders",
    };
}

void trayser::SlangCompiler::LoadVertexFragmentShader(const char* slangFileName, Slang::ComPtr<slang::IBlob>& vsSpirv, Slang::ComPtr<slang::IBlob>& fsSpirv)
{
    Slang::ComPtr<slang::ISession> session;
    CreateSlangSession(session);

    slang::IModule* module = LoadModule(slangFileName, session);
    if (!module)
        return;

    Slang::ComPtr<slang::IEntryPoint> vsEntryPoint;
    module->findEntryPointByName("vsMain", vsEntryPoint.writeRef());
    Slang::ComPtr<slang::IEntryPoint> fsEntryPoint;
    module->findEntryPointByName("fsMain", fsEntryPoint.writeRef());

    std::vector<slang::IComponentType*> componentTypes;
    componentTypes.push_back(module);
    componentTypes.push_back(vsEntryPoint);
    componentTypes.push_back(fsEntryPoint);

    Slang::ComPtr<slang::IComponentType> composedProgram;
    {
        Slang::ComPtr<slang::IBlob> diagnosticsBlob;
        SlangResult result = session->createCompositeComponentType(
            componentTypes.data(),
            componentTypes.size(),
            composedProgram.writeRef(),
            diagnosticsBlob.writeRef());
        if (result == SLANG_FAIL)
        {
            return;
        }
    }

    Slang::ComPtr<slang::IBlob> diagnosticBlob;
    composedProgram->getEntryPointCode(0, 0, vsSpirv.writeRef(), diagnosticBlob.writeRef());
    Diagnose(diagnosticBlob);

    diagnosticBlob = nullptr;
    composedProgram->getEntryPointCode(1, 0, fsSpirv.writeRef(), diagnosticBlob.writeRef());
    Diagnose(diagnosticBlob);
}

void trayser::SlangCompiler::LoadComputeShader(const char* slangFileName, Slang::ComPtr<slang::IBlob>& spirv)
{
    Slang::ComPtr<slang::ISession> session;
    CreateSlangSession(session);

    slang::IModule* module = LoadModule(slangFileName, session);
    if (!module)
        return;

    Slang::ComPtr<slang::IEntryPoint> entryPoint;
    module->findEntryPointByName("csMain", entryPoint.writeRef());

    std::vector<slang::IComponentType*> componentTypes;
    componentTypes.push_back(module);
    componentTypes.push_back(entryPoint);

    Slang::ComPtr<slang::IComponentType> composedProgram;
    {
        Slang::ComPtr<slang::IBlob> diagnosticsBlob;
        SlangResult result = session->createCompositeComponentType(
            componentTypes.data(),
            componentTypes.size(),
            composedProgram.writeRef(),
            diagnosticsBlob.writeRef());
        if (result == SLANG_FAIL)
        {
            return;
        }
    }

    Slang::ComPtr<slang::IBlob> diagnosticBlob;
    composedProgram->getEntryPointCode(0, 0, spirv.writeRef(), diagnosticBlob.writeRef());
    Diagnose(diagnosticBlob);
}

bool trayser::SlangCompiler::LoadShaderModule(const char* spirvFileName, VkShaderModule& outModule)
{
    std::string filePath = FindExistingFile((std::string(spirvFileName) + ".spv").c_str());

    if (filePath.empty())
    {
        return false;
    }

    // open the file. With cursor at the end
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        return false;
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<u32> buffer(fileSize / sizeof(u32));
    file.seekg(0);
    file.read((char*)buffer.data(), fileSize);
    file.close();
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.codeSize = buffer.size() * sizeof(u32);
    createInfo.pCode = buffer.data();
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(g_engine.m_device.m_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
    {
        return false;
    }

    outModule = shaderModule;
    return true;
}

void trayser::SlangCompiler::CreateSlangSession(Slang::ComPtr<slang::ISession>& session) const
{
    slang::TargetDesc targetDesc{};
    targetDesc.format = SLANG_SPIRV;
    targetDesc.profile = m_slangGlobalSession->findProfile("spirv_1_3");
    targetDesc.flags = 0;

    slang::SessionDesc sessionDesc{};
    sessionDesc.targets = &targetDesc;
    sessionDesc.targetCount = 1;
    sessionDesc.compilerOptionEntryCount = 0;
    sessionDesc.searchPaths = m_searchPaths.data();
    sessionDesc.searchPathCount = m_searchPaths.size();

    m_slangGlobalSession->createSession(sessionDesc, session.writeRef());
}

slang::IModule* trayser::SlangCompiler::LoadModule(const char* slangFileName, Slang::ComPtr<slang::ISession>& session)
{
    slang::IModule* module = nullptr;
    Slang::ComPtr<slang::IBlob> diagnosticBlob;
    module = session->loadModule(slangFileName, diagnosticBlob.writeRef());
    Diagnose(diagnosticBlob);
    return module;
}

std::string trayser::SlangCompiler::FindExistingFile(const char* fileName)
{
    for (auto& path : m_searchPaths)
    {
        std::string fullPath = std::string(path) + "/" + fileName;
        if (std::filesystem::exists(fullPath))
            return fullPath;
    }
    return "";
}

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
    Slang::ComPtr<slang::IBlob> vsSpirv, fsSpirv;
	g_engine.m_compiler.LoadVertexFragmentShader(m_name.c_str(), vsSpirv, fsSpirv);

	if (!vsSpirv || !fsSpirv)
	{
		fmt::println("Failed to load PBR shaders");
		return;
	}

    VkShaderModule vsModule = CreateShaderModule(g_engine.m_device.m_device, vsSpirv);
    VkShaderModule fsModule = CreateShaderModule(g_engine.m_device.m_device, fsSpirv);

    VkPushConstantRange pushConst{};
    pushConst.offset = 0;
    pushConst.size = sizeof(gpu::PushConstants);
    pushConst.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
    pipeline_layout_info.pPushConstantRanges = &pushConst;
    pipeline_layout_info.pushConstantRangeCount = 2;
    pipeline_layout_info.pSetLayouts = &g_engine.m_singleImageDescriptorLayout;
    pipeline_layout_info.setLayoutCount = 1;
    VK_CHECK(vkCreatePipelineLayout(g_engine.m_device.m_device, &pipeline_layout_info, nullptr, &m_layout));

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
    rasterization.cullMode = VK_CULL_MODE_FRONT_BIT;
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
    rendering.pColorAttachmentFormats = &g_engine.m_gBuffer.colorBuffer.imageFormat;
    rendering.depthAttachmentFormat = g_engine.m_gBuffer.depthBuffer.imageFormat;

    VkPipelineShaderStageCreateInfo vsStage{};
    vsStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vsStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vsStage.module = vsModule;
    vsStage.pName = "main";

    VkPipelineShaderStageCreateInfo fsStage{};
    fsStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fsStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fsStage.module = fsModule;
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

    vkDestroyShaderModule(g_engine.m_device.m_device, vsModule, nullptr);
    vkDestroyShaderModule(g_engine.m_device.m_device, fsModule, nullptr);
}

void trayser::PBRPipeline::Update()
{
    auto cmd = g_engine.m_device.GetCmd();

    VkExtent2D extent = { g_engine.m_gBuffer.colorBuffer.imageExtent.width, g_engine.m_gBuffer.colorBuffer.imageExtent.height };

    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(g_engine.m_gBuffer.colorBuffer.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(g_engine.m_gBuffer.depthBuffer.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkRenderingInfo renderInfo = vkinit::rendering_info(extent, &colorAttachment, &depthAttachment);
    vkCmdBeginRendering(cmd, &renderInfo);

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
            pushConst.camPos = glm::vec4(g_engine.m_camera.m_position, 1.0f);

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

    VkExtent2D extent = { g_engine.m_gBuffer.colorBuffer.imageExtent.width, g_engine.m_gBuffer.colorBuffer.imageExtent.height };

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
    writer.WriteImage(0, g_engine.m_gBuffer.colorBuffer.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    writer.WriteImage(1, g_engine.m_gBuffer.colorBuffer.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
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

    Slang::ComPtr<slang::IBlob> spirv;
    g_engine.m_compiler.LoadComputeShader(m_name.c_str(), spirv);

    if (!spirv)
    {
        fmt::println("Failed to load tonemap shaders");
        return;
    }

    VkShaderModule module = CreateShaderModule(g_engine.m_device.m_device, spirv);

    VkPipelineShaderStageCreateInfo stageinfo{};
    stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageinfo.pNext = nullptr;
    stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageinfo.module = module;
    stageinfo.pName = "main";

    VkComputePipelineCreateInfo computePipelineCreateInfo{};
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.pNext = nullptr;
    computePipelineCreateInfo.layout = m_layout;
    computePipelineCreateInfo.stage = stageinfo;

    VK_CHECK(vkCreateComputePipelines(g_engine.m_device.m_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &m_pipeline));

    vkDestroyShaderModule(g_engine.m_device.m_device, module, nullptr);
}

void trayser::TonemapPipeline::Update()
{
    auto cmd = g_engine.m_device.GetCmd();

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_layout, 0, 1, &m_descriptorSet, 0, nullptr);

    PushConstantComp pc{};
    pc.tonemapMode = g_engine.m_tonemapMode;

    vkCmdPushConstants(cmd, m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantComp), &pc);

    VkExtent2D extent = { g_engine.m_gBuffer.colorBuffer.imageExtent.width, g_engine.m_gBuffer.colorBuffer.imageExtent.height };

    // execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
    vkCmdDispatch(cmd, std::ceil(extent.width / 16.0), std::ceil(extent.height / 16.0), 1);
}
