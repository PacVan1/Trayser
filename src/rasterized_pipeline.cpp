#include <pch.h>

#include <rasterized_pipeline.h>
#include <engine.h>

trayser::RasterizedPipeline::RasterizedPipeline()
{
    m_name = "rasterized";
    m_canHotReload = true;
}

void trayser::RasterizedPipeline::Load()
{
    VkShaderModule module = g_engine.m_compiler.CompileAll(m_name.c_str());
    if (module == VK_NULL_HANDLE)
        return;

    VkPushConstantRange pushConst{};
    pushConst.offset = 0;
    pushConst.size = sizeof(gpu::RasterPushConstants);
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
    vsStage.module = module;
    vsStage.pName = "vsMain";

    VkPipelineShaderStageCreateInfo fsStage{};
    fsStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fsStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fsStage.module = module;
    fsStage.pName = "fsMain";

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

    vkDestroyShaderModule(g_engine.m_device.m_device, module, nullptr);
}

void trayser::RasterizedPipeline::Update()
{
    auto cmd = g_engine.m_device.GetCmd();

    VkExtent2D extent = { g_engine.m_gBuffer.colorImage.imageExtent.width, g_engine.m_gBuffer.colorImage.imageExtent.height };

    VkRenderingAttachmentInfo colorAttachment = RenderingAttachmentInfo();
    colorAttachment.imageView = g_engine.m_gBuffer.colorImage.imageView;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingAttachmentInfo depthAttachment = RenderingAttachmentInfo();
    depthAttachment.imageView = g_engine.m_gBuffer.depthImage.imageView;
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
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
        VkBuffer vertexBuffers[] = { g_engine.m_meshPool.Get(render.mesh).vertexBuffer.buffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindIndexBuffer(cmd, g_engine.m_meshPool.Get(render.mesh).indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);

        for (auto& prim : g_engine.m_meshPool.Get(render.mesh).primitives)
        {
            //bind a texture
            VkDescriptorSet imageSet = g_engine.m_device.GetFrame().descriptors.Allocate(g_engine.m_device.m_device, g_engine.m_singleImageDescriptorLayout);
            {
                DescriptorWriter writer;
                writer.WriteImage(0, g_engine.m_meshPool.Get(render.mesh).materials[prim.materialId].baseColor ? g_engine.m_meshPool.Get(render.mesh).materials[prim.materialId].baseColor->imageView : g_engine.m_defaultMaterial.baseColor->imageView, g_engine.m_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                writer.WriteImage(1, g_engine.m_meshPool.Get(render.mesh).materials[prim.materialId].normalMap ? g_engine.m_meshPool.Get(render.mesh).materials[prim.materialId].normalMap->imageView : g_engine.m_defaultMaterial.normalMap->imageView, g_engine.m_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                writer.WriteImage(2, g_engine.m_meshPool.Get(render.mesh).materials[prim.materialId].metallicRoughness ? g_engine.m_meshPool.Get(render.mesh).materials[prim.materialId].metallicRoughness->imageView : g_engine.m_defaultMaterial.metallicRoughness->imageView, g_engine.m_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                writer.WriteImage(3, g_engine.m_meshPool.Get(render.mesh).materials[prim.materialId].occlusion ? g_engine.m_meshPool.Get(render.mesh).materials[prim.materialId].occlusion->imageView : g_engine.m_defaultMaterial.occlusion->imageView, g_engine.m_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                writer.WriteImage(4, g_engine.m_meshPool.Get(render.mesh).materials[prim.materialId].emissive ? g_engine.m_meshPool.Get(render.mesh).materials[prim.materialId].emissive->imageView : g_engine.m_defaultMaterial.emissive->imageView, g_engine.m_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

                writer.UpdateSet(g_engine.m_device.m_device, imageSet);
            }

            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_layout, 0, 1, &imageSet, 0, nullptr);

            gpu::RasterPushConstants pushConsts{};
            pushConsts.sceneRef = g_engine.m_gpuSceneAddr;
            pushConsts.transform = tf.matrix;
            pushConsts.renderMode.x = g_engine.m_renderMode;

            vkCmdPushConstants(g_engine.m_device.GetCmd(),
                m_layout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(gpu::RasterPushConstants),
                &pushConsts);

            vkCmdDrawIndexed(cmd, prim.indexCount, 1, prim.baseIndex, 0, 0);
        }
    }

    vkCmdEndRendering(cmd);
}