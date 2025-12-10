#include <pch.h>

#include <engine.h>
#include <equi2cubemap_pipeline.h>

trayser::Equi2CubemapPipeline::Equi2CubemapPipeline()
{
	m_name = "equi2cubemap";
	m_canHotReload = false;
}

void trayser::Equi2CubemapPipeline::Load(VkShaderModule module)
{
    DescriptorLayoutBuilder builder;
    builder.AddBinding(BindingPoints_SrcImage, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    builder.AddBinding(BindingPoints_DstCubemap, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    m_descriptorSetLayout = builder.Build(g_engine.m_device.m_device, VK_SHADER_STAGE_COMPUTE_BIT);
    m_descriptorSet = g_engine.m_globalDescriptorAllocator.Allocate(g_engine.m_device.m_device, m_descriptorSetLayout); 

    VkPushConstantRange pushConstant{};
    pushConstant.offset = 0;
    pushConstant.size = sizeof(PushConstantComp);
    pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkPipelineLayoutCreateInfo layoutCreateInfo = PipelineLayoutCreateInfo();
    layoutCreateInfo.pSetLayouts            = &m_descriptorSetLayout;
    layoutCreateInfo.setLayoutCount         = 1;
    layoutCreateInfo.pushConstantRangeCount = 0;
    layoutCreateInfo.pPushConstantRanges    = nullptr;
    VK_CHECK(vkCreatePipelineLayout(g_engine.m_device.m_device, &layoutCreateInfo, nullptr, &m_layout));

    VkPipelineShaderStageCreateInfo stageCreateInfo = PipelineShaderStageCreateInfo();
    stageCreateInfo.stage   = VK_SHADER_STAGE_COMPUTE_BIT;
    stageCreateInfo.module  = module;
    stageCreateInfo.pName   = "main";

    VkComputePipelineCreateInfo pipCreateInfo = ComputePipelineCreateInfo();
    pipCreateInfo.layout = m_layout;
    pipCreateInfo.stage = stageCreateInfo;
    VK_CHECK(vkCreateComputePipelines(g_engine.m_device.m_device, VK_NULL_HANDLE, 1, &pipCreateInfo, nullptr, &m_pipeline));

    vkDestroyShaderModule(g_engine.m_device.m_device, module, nullptr);
}

void trayser::Equi2CubemapPipeline::Update()
{
    assert(m_srcImage && m_dstCubemap);

    VkCommandBuffer cmd;
    g_engine.m_device.BeginOneTimeSubmit(cmd);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);

    DescriptorWriter writer;
    writer.WriteImage(BindingPoints_SrcImage, m_srcImage->imageView, g_engine.m_sampler, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    writer.WriteImage(BindingPoints_DstCubemap, m_dstCubemap->imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    writer.UpdateSet(g_engine.m_device.m_device, m_descriptorSet);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_layout, 0, 1, &m_descriptorSet, 0, nullptr);

    VkExtent2D extent = { m_dstCubemap->imageExtent.width, m_dstCubemap->imageExtent.height };
    vkCmdDispatch(cmd, std::ceil(extent.width / 16.0), std::ceil(extent.height / 16.0), 6);

    g_engine.m_device.EndOneTimeSubmit();
}

VkShaderModule trayser::Equi2CubemapPipeline::Compile()
{
    return g_engine.m_compiler.CompileAll(m_name.c_str());
}

void trayser::Equi2CubemapPipeline::SetImage(AllocatedImage& srcImage, AllocatedImage& dstCubemap)
{
    m_srcImage = &srcImage;
    m_dstCubemap = &dstCubemap;
}