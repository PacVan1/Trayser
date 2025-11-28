#include <pch.h>
#include <pipelines.h>
#include <fstream>
#include <initializers.h>
#include <engine.h>
#include <filesystem>

bool vkutil::LoadShaderModule(const char* filePath, VkDevice device, VkShaderModule* outShaderModule)
{
    // open the file. With cursor at the end
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);

    if (!file.is_open()) 
    {
        return false;
    }

    // find what the size of the file is by looking up the location of the cursor
    // because the cursor is at the end, it gives the size directly in bytes
    size_t fileSize = (size_t)file.tellg();

    // spirv expects the buffer to be on uint32, so make sure to reserve a int
    // vector big enough for the entire file
    std::vector<u32> buffer(fileSize / sizeof(u32));

    // put file cursor at beginning
    file.seekg(0);

    // load the entire file into the buffer
    file.read((char*)buffer.data(), fileSize);

    // now that the file is loaded into the buffer, we can close it
    file.close();

    // create a new shader module, using the buffer we loaded
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pNext = nullptr;

    // codeSize has to be in bytes, so multply the ints in the buffer by size of
    // int to know the real size of the buffer
    createInfo.codeSize = buffer.size() * sizeof(u32);
    createInfo.pCode = buffer.data();

    // check that the creation goes well.
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) 
    {
        return false;
    }

    *outShaderModule = shaderModule;
    return true;
}

void vkutil::PipelineBuilder::Clear()
{
    // clear all of the structs we need back to 0 with their correct stype
    m_inputAssembly = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    m_rasterizer = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    m_colorBlendAttachment = {};
    m_multisampling = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    m_pipelineLayout = {};
    m_depthStencil = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    m_renderInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    m_vertexInputInfo = nullptr;
    m_shaderStages.clear();
}

VkPipeline vkutil::PipelineBuilder::Build(VkDevice device)
{
    // make viewport state from our stored viewport and scissor.
    // at the moment we wont support multiple viewports or scissors
    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.pNext = nullptr;

    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // setup dummy color blending. We arent using transparent objects yet
    // the blending is just "no blend", but we do write to the color attachment
    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.pNext = nullptr;

    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &m_colorBlendAttachment;

    // completely clear VertexInputStateCreateInfo, as we have no need for it
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

    // build the actual pipeline
    // 
    // we now use all of the info structs we have been writing into into this one
    // to create the pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo = { .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    // connect the renderInfo to the pNext extension mechanism
    pipelineInfo.pNext = &m_renderInfo;

    pipelineInfo.stageCount = (uint32_t)m_shaderStages.size();
    pipelineInfo.pStages = m_shaderStages.data();
    pipelineInfo.pVertexInputState = m_vertexInputInfo ? m_vertexInputInfo : &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &m_inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &m_rasterizer;
    pipelineInfo.pMultisampleState = &m_multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDepthStencilState = &m_depthStencil;
    pipelineInfo.layout = m_pipelineLayout;

    VkDynamicState state[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamicInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynamicInfo.pDynamicStates = &state[0];
    dynamicInfo.dynamicStateCount = 2;

    pipelineInfo.pDynamicState = &dynamicInfo;

    // its easy to error out on create graphics pipeline, so we handle it a bit
    // better than the common VK_CHECK case
    VkPipeline newPipeline;
    VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline);
    if (result != VK_SUCCESS) 
    {
        fmt::println("failed to create pipeline");
        return VK_NULL_HANDLE; // failed to create graphics pipeline
    }
    else 
    {
        return newPipeline;
    }
}

void vkutil::PipelineBuilder::SetVertexInputInfo(VkPipelineVertexInputStateCreateInfo* info)
{
    m_vertexInputInfo = info;
}

void vkutil::PipelineBuilder::SetShaders(VkShaderModule vertexShader, VkShaderModule fragmentShader)
{
    m_shaderStages.clear();

    m_shaderStages.push_back(
        vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, vertexShader));

    m_shaderStages.push_back(
        vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader));
}

void vkutil::PipelineBuilder::SetInputTopology(VkPrimitiveTopology topology)
{
    m_inputAssembly.topology = topology;
    // we are not going to use primitive restart on the entire tutorial so leave
    // it on false
    m_inputAssembly.primitiveRestartEnable = VK_FALSE;
}

void vkutil::PipelineBuilder::SetPolygonMode(VkPolygonMode mode)
{
    m_rasterizer.polygonMode = mode;
    m_rasterizer.lineWidth = 1.f;
}

void vkutil::PipelineBuilder::SetCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace)
{
    m_rasterizer.cullMode = cullMode;
    m_rasterizer.frontFace = frontFace;
}

void vkutil::PipelineBuilder::SetMultisamplingNone()
{
    m_multisampling.sampleShadingEnable = VK_FALSE;
    // multisampling defaulted to no multisampling (1 sample per pixel)
    m_multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    m_multisampling.minSampleShading = 1.0f;
    m_multisampling.pSampleMask = nullptr;
    // no alpha to coverage either
    m_multisampling.alphaToCoverageEnable = VK_FALSE;
    m_multisampling.alphaToOneEnable = VK_FALSE;
}

void vkutil::PipelineBuilder::DisableBlending()
{
    // default write mask
    m_colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    // no blending
    m_colorBlendAttachment.blendEnable = VK_FALSE;
}

void vkutil::PipelineBuilder::SetColorAttachmentFormat(VkFormat format)
{
    m_colorAttachmentformat = format;
    // connect the format to the renderInfo  structure
    m_renderInfo.colorAttachmentCount = 1;
    m_renderInfo.pColorAttachmentFormats = &m_colorAttachmentformat;
}

void vkutil::PipelineBuilder::SetDepthFormat(VkFormat format)
{
    m_renderInfo.depthAttachmentFormat = format;
}

void vkutil::PipelineBuilder::DisableDepthTest()
{
    m_depthStencil.depthTestEnable = VK_FALSE;
    m_depthStencil.depthWriteEnable = VK_FALSE;
    m_depthStencil.depthCompareOp = VK_COMPARE_OP_NEVER;
    m_depthStencil.depthBoundsTestEnable = VK_FALSE;
    m_depthStencil.stencilTestEnable = VK_FALSE;
    m_depthStencil.front = {};
    m_depthStencil.back = {};
    m_depthStencil.minDepthBounds = 0.f;
    m_depthStencil.maxDepthBounds = 1.f;
}

void vkutil::PipelineBuilder::EnableDepthTest(bool depthWriteEnable, VkCompareOp op)
{
    m_depthStencil.depthTestEnable = VK_TRUE;
    m_depthStencil.depthWriteEnable = depthWriteEnable;
    m_depthStencil.depthCompareOp = op;
    m_depthStencil.depthBoundsTestEnable = VK_FALSE;
    m_depthStencil.stencilTestEnable = VK_FALSE;
    m_depthStencil.front = {};
    m_depthStencil.back = {};
    m_depthStencil.minDepthBounds = 0.f;
    m_depthStencil.maxDepthBounds = 1.f;
}

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

void trayser::Pipeline::Destroy() const
{
	vkDestroyPipelineLayout(VulkanEngine::Get().m_device, m_layout, nullptr);
	vkDestroyPipeline(VulkanEngine::Get().m_device, m_pipeline, nullptr);
}

void trayser::Pipeline::ReloadIfChanged()
{
	if (!m_canHotReload)
		return;

    const char* searchPaths[] =
    {
        "C:/Projects/BUas/Y2B/Trayser/shaders"
    };

    std::string fileName = std::string(searchPaths[0]) + "/" + m_name + ".slang";

	if (!std::filesystem::exists(fileName))
		return;

    auto currentLastWriteTime = std::filesystem::last_write_time(fileName);
    // Shader source has changed
    if (m_lastWriteTime < currentLastWriteTime)
    {   
        m_lastWriteTime = currentLastWriteTime;

        vkQueueWaitIdle(VulkanEngine::Get().m_graphicsQueue);
        Load();
    }
}

static void LoadSpirv(const char* spirvFileName, VkShaderModule& outShaderModule)
{
    // open the file. With cursor at the end
    std::ifstream file(spirvFileName, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        return;
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
    if (vkCreateShaderModule(VulkanEngine::Get().m_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
    {
        return;
    }

    outShaderModule = shaderModule;
}

void trayser::PBRPipeline::Init()
{
    const char* searchPaths[] =
    {
        "C:/Projects/BUas/Y2B/Trayser/shaders"
    };

    m_name = "pbr";
    m_canHotReload = true;

    std::string fileName = std::string(searchPaths[0]) + "/" + m_name + ".slang";

    m_lastWriteTime = std::filesystem::last_write_time(fileName);
    Load();
}

void trayser::PBRPipeline::Load()
{
    Slang::ComPtr<slang::IBlob> vsSpirv, fsSpirv;
	VulkanEngine::Get().m_compiler.LoadVertexFragmentShader(m_name.c_str(), vsSpirv, fsSpirv);

	if (!vsSpirv || !fsSpirv)
	{
		fmt::println("Failed to load PBR shaders");
		return;
	}

    VkShaderModule vsModule = CreateShaderModule(VulkanEngine::Get().m_device, vsSpirv);
    VkShaderModule fsModule = CreateShaderModule(VulkanEngine::Get().m_device, fsSpirv);

    VkPushConstantRange pushConst{};
    pushConst.offset = 0;
    pushConst.size = sizeof(gpu::PushConstants);
    pushConst.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
    pipeline_layout_info.pPushConstantRanges = &pushConst;
    pipeline_layout_info.pushConstantRangeCount = 2;
    pipeline_layout_info.pSetLayouts = &VulkanEngine::Get().m_singleImageDescriptorLayout;
    pipeline_layout_info.setLayoutCount = 1;
    VK_CHECK(vkCreatePipelineLayout(VulkanEngine::Get().m_device, &pipeline_layout_info, nullptr, &m_layout));

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
    rendering.pColorAttachmentFormats = &VulkanEngine::Get().m_renderImage.imageFormat;
    rendering.depthAttachmentFormat = VulkanEngine::Get().m_depthImage.imageFormat;

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

    if (vkCreateGraphicsPipelines(VulkanEngine::Get().m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS)
    {
        fmt::println("Failed to create pipeline");
        return;
    }

    vkDestroyShaderModule(VulkanEngine::Get().m_device, vsModule, nullptr);
    vkDestroyShaderModule(VulkanEngine::Get().m_device, fsModule, nullptr);
}

void trayser::PBRPipeline::Update()
{
    auto& cmd = VulkanEngine::Get().GetCurrentFrame().commandBuffer;

    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(VulkanEngine::Get().m_renderImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(VulkanEngine::Get().m_depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkRenderingInfo renderInfo = vkinit::rendering_info(VulkanEngine::Get().m_renderExtent, &colorAttachment, &depthAttachment);
    vkCmdBeginRendering(cmd, &renderInfo);

    //set dynamic viewport and scissor
    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = VulkanEngine::Get().m_renderExtent.width;
    viewport.height = VulkanEngine::Get().m_renderExtent.height;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;

    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = VulkanEngine::Get().m_renderExtent.width;
    scissor.extent.height = VulkanEngine::Get().m_renderExtent.height;

    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

    auto view = VulkanEngine::Get().m_scene.m_registry.view<WorldTransform, RenderComponent>();
    for (const auto& [ent, tf, render] : view.each())
    {
        for (auto& prim : render.mesh->primitives)
        {
            //bind a texture
            VkDescriptorSet imageSet = VulkanEngine::Get().GetCurrentFrame().descriptors.Allocate(VulkanEngine::Get().m_device, VulkanEngine::Get().m_singleImageDescriptorLayout);
            {
                DescriptorWriter writer;
                writer.WriteImage(0, render.mesh->materials[prim.materialId].baseColor ? render.mesh->materials[prim.materialId].baseColor->imageView : VulkanEngine::Get().m_defaultMaterial.baseColor->imageView, VulkanEngine::Get().m_defaultSamplerNearest, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                writer.WriteImage(1, render.mesh->materials[prim.materialId].normalMap ? render.mesh->materials[prim.materialId].normalMap->imageView : VulkanEngine::Get().m_defaultMaterial.normalMap->imageView, VulkanEngine::Get().m_defaultSamplerNearest, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                writer.WriteImage(2, render.mesh->materials[prim.materialId].metallicRoughness ? render.mesh->materials[prim.materialId].metallicRoughness->imageView : VulkanEngine::Get().m_defaultMaterial.metallicRoughness->imageView, VulkanEngine::Get().m_defaultSamplerNearest, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                writer.WriteImage(3, render.mesh->materials[prim.materialId].occlusion ? render.mesh->materials[prim.materialId].occlusion->imageView : VulkanEngine::Get().m_defaultMaterial.occlusion->imageView, VulkanEngine::Get().m_defaultSamplerNearest, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                writer.WriteImage(4, render.mesh->materials[prim.materialId].emissive ? render.mesh->materials[prim.materialId].emissive->imageView : VulkanEngine::Get().m_defaultMaterial.emissive->imageView, VulkanEngine::Get().m_defaultSamplerNearest, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

                writer.UpdateSet(VulkanEngine::Get().m_device, imageSet);
            }

            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_layout, 0, 1, &imageSet, 0, nullptr);

            gpu::PushConstants pushConst{};
            glm::mat4 projection = VulkanEngine::Get().m_camera.m_proj;
            projection[1][1] *= -1;
            pushConst.renderMode = glm::ivec4(VulkanEngine::Get().m_renderMode, 0, 0, 0);
            pushConst.camPos = glm::vec4(VulkanEngine::Get().m_camera.m_position, 1.0f);

            pushConst.viewProj = projection * VulkanEngine::Get().m_camera.m_view;
            pushConst.model = tf.matrix;

            VkBuffer vertexBuffers[] = { render.mesh->vertexBuffer.buffer };
            VkDeviceSize offsets[] = { 0 };
            vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);

            vkCmdPushConstants(cmd, m_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(gpu::PushConstants), &pushConst);
            //vkCmdBindIndexBuffer(cmd, render.mesh->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

            //vkCmdDrawIndexed(cmd, render.mesh->surfaces[0].count, 1, render.mesh->surfaces[0].startIndex, 0, 0);
            vkCmdDraw(cmd, prim.vertexCount, 1, 0, 0);
        }
    }

    vkCmdEndRendering(cmd);
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

void trayser::SlangCompiler::CreateSlangSession(Slang::ComPtr<slang::ISession>& session) const
{
    slang::TargetDesc targetDesc{};
    targetDesc.format                    = SLANG_SPIRV;
    targetDesc.profile                   = m_slangGlobalSession->findProfile("spirv_1_3");
    targetDesc.flags                     = 0;
    slang::SessionDesc sessionDesc{};
    sessionDesc.targets                  = &targetDesc;
    sessionDesc.targetCount              = 1;
    sessionDesc.compilerOptionEntryCount = 0;

    const char* searchPaths[] =
    {
        "C:/Projects/BUas/Y2B/Trayser/shaders"
    };

    sessionDesc.searchPaths = searchPaths;
    sessionDesc.searchPathCount = 1;
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

void trayser::BackgroundPipeline::Init()
{
    //const char* searchPaths[] =
    //{
    //    "C:/Projects/BUas/Y2B/Trayser/shaders"
    //};

    m_name = "sky";
    m_canHotReload = false;


    //std::string fileName = std::string(searchPaths[0]) + "/" + m_name + ".slang";
    //m_lastWriteTime = std::filesystem::last_write_time(fileName);

    Load();
}

void trayser::BackgroundPipeline::Load()
{
    VkPipelineLayoutCreateInfo computeLayout{};
    computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    computeLayout.pNext = nullptr;
    computeLayout.pSetLayouts = &VulkanEngine::Get().m_renderImageDescriptorLayout;
    computeLayout.setLayoutCount = 1;

    VkPushConstantRange pushConstant{};
    pushConstant.offset = 0;
    pushConstant.size = sizeof(ComputePushConstants);
    pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    computeLayout.pPushConstantRanges = &pushConstant;
    computeLayout.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(VulkanEngine::Get().m_device, &computeLayout, nullptr, &m_layout));

    // Layout code
    VkShaderModule computeDrawShader;
    if (!vkutil::LoadShaderModule("../shaders/sky.comp.spv", VulkanEngine::Get().m_device, &computeDrawShader))
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

    VK_CHECK(vkCreateComputePipelines(VulkanEngine::Get().m_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &m_pipeline));

    vkDestroyShaderModule(VulkanEngine::Get().m_device, computeDrawShader, nullptr);
}

void trayser::BackgroundPipeline::Update()
{
    auto& cmd = VulkanEngine::Get().GetCurrentFrame().commandBuffer;

    // bind the gradient drawing compute pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);

    // bind the descriptor set containing the draw image for the compute pipeline
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_layout, 0, 1, &VulkanEngine::Get().m_renderImageDescriptors, 0, nullptr);

    ComputePushConstants pc;
    pc.data1 = glm::vec4(0.1, 0.2, 0.4, 0.97);

    vkCmdPushConstants(cmd, m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &pc);

    // execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
    vkCmdDispatch(cmd, std::ceil(VulkanEngine::Get().m_renderExtent.width / 16.0), std::ceil(VulkanEngine::Get().m_renderExtent.height / 16.0), 1);
}
