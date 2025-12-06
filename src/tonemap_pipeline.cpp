#include <pch.h>

#include <tonemap_pipeline.h>
#include <compiler.h>
#include <engine.h>

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
    compileInfo.fileName = m_name.c_str();
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