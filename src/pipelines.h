#pragma once 

#include <types.h>

namespace vkutil 
{

bool LoadShaderModule(const char* filePath, VkDevice device, VkShaderModule* outShaderModule);

class PipelineBuilder
{
public:
    PipelineBuilder() { Clear(); }

    void Clear();
    VkPipeline Build(VkDevice device);
    void SetShaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);
    void SetInputTopology(VkPrimitiveTopology topology);
    void SetPolygonMode(VkPolygonMode mode);
    void SetCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace);
    void SetMultisamplingNone();
    void DisableBlending();
    void SetColorAttachmentFormat(VkFormat format);
    void SetDepthFormat(VkFormat format);
    void DisableDepthTest();

public:
    std::vector<VkPipelineShaderStageCreateInfo>    m_shaderStages;
    VkPipelineInputAssemblyStateCreateInfo          m_inputAssembly;
    VkPipelineRasterizationStateCreateInfo          m_rasterizer;
    VkPipelineColorBlendAttachmentState             m_colorBlendAttachment;
    VkPipelineMultisampleStateCreateInfo            m_multisampling;
    VkPipelineLayout                                m_pipelineLayout;
    VkPipelineDepthStencilStateCreateInfo           m_depthStencil;
    VkPipelineRenderingCreateInfo                   m_renderInfo;
    VkFormat                                        m_colorAttachmentformat;
};

} // namespace vkutil