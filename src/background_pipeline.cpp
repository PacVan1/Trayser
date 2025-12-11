#include <pch.h>

#include <engine.h>
#include <background_pipeline.h>

trayser::BackgroundPipeline::BackgroundPipeline()
{
    m_name = "sky.comp";
    m_canHotReload = false;
}

void trayser::BackgroundPipeline::Load(VkShaderModule module)
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

void trayser::BackgroundPipeline::Update()
{
    auto cmd = g_engine.m_renderer.GetCmdBuffer();

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

VkShaderModule trayser::BackgroundPipeline::Compile()
{
    return g_engine.m_compiler.LoadSpirV(m_name.c_str());
}
