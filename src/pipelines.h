#pragma once 

#include <types.h>
#include <chrono>
#include <slang/slang.h>
#include <slang/slang-com-ptr.h>
#include <vector>

namespace vkutil 
{

bool LoadShaderModule(const char* filePath, VkDevice device, VkShaderModule* outShaderModule);

class PipelineBuilder
{
public:
    PipelineBuilder() { Clear(); }

    void Clear();
    VkPipeline Build(VkDevice device);
    void SetVertexInputInfo(VkPipelineVertexInputStateCreateInfo* info);
    void SetShaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);
    void SetInputTopology(VkPrimitiveTopology topology);
    void SetPolygonMode(VkPolygonMode mode);
    void SetCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace);
    void SetMultisamplingNone();
    void DisableBlending();
    void SetColorAttachmentFormat(VkFormat format);
    void SetDepthFormat(VkFormat format);
    void DisableDepthTest();
    void EnableDepthTest(bool depthWriteEnable, VkCompareOp op);

public:
    std::vector<VkPipelineShaderStageCreateInfo>    m_shaderStages;
    VkPipelineVertexInputStateCreateInfo*           m_vertexInputInfo = nullptr;
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

namespace trayser
{

class SlangCompiler
{
public:
    void Init();
    void LoadVertexFragmentShader(const char* slangFileName, Slang::ComPtr<slang::IBlob>& vsSpirv, Slang::ComPtr<slang::IBlob>& fsSpirv);

private:
    void CreateSlangSession(Slang::ComPtr<slang::ISession>& session) const;
    slang::IModule* LoadModule(const char* slangFileName, Slang::ComPtr<slang::ISession>& session);

public:
	Slang::ComPtr<slang::IGlobalSession> m_slangGlobalSession;
    slang::SessionDesc                   m_sessionDesc;
    slang::TargetDesc                    m_targetDesc;
};

class Pipeline
{
public:
	std::chrono::time_point <std::chrono::file_clock> m_lastWriteTime;
    std::string         m_name;
    VkPipelineLayout    m_layout;
    VkPipeline          m_pipeline;
    bool                m_canHotReload = false;

public:
    void Destroy() const;
    void ReloadIfChanged();

    virtual void Init() = 0;
    virtual void Load() = 0;
    virtual void Update() = 0;
};

class PBRPipeline : public Pipeline
{
public:
    virtual void Init() override;
    virtual void Load() override;
    virtual void Update() override;
};

class BackgroundPipeline : public Pipeline
{
public:
    virtual void Init() override;
    virtual void Load() override;
    virtual void Update() override;
};

}