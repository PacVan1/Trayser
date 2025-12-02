#pragma once 

#include <types.h>
#include <chrono>
#include <slang/slang.h>
#include <slang/slang-com-ptr.h>
#include <vector>

namespace trayser
{

class SlangCompiler
{
public:
    void Init();
    void LoadVertexFragmentShader(const char* slangFileName, Slang::ComPtr<slang::IBlob>& vsSpirv, Slang::ComPtr<slang::IBlob>& fsSpirv);
    bool LoadShaderModule(const char* spirvFileName, VkShaderModule& outModule);
    std::string FindExistingFile(const char* fileName);

private:
    void CreateSlangSession(Slang::ComPtr<slang::ISession>& session) const;
    slang::IModule* LoadModule(const char* slangFileName, Slang::ComPtr<slang::ISession>& session);

public:
	Slang::ComPtr<slang::IGlobalSession> m_slangGlobalSession;
    slang::SessionDesc                   m_sessionDesc;
    slang::TargetDesc                    m_targetDesc;
	std::vector<const char*>             m_searchPaths;
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
    void Init();
    void Destroy() const;
    void ReloadIfChanged();

    virtual void Load() = 0;
    virtual void Update() = 0;
};

class PBRPipeline final : public Pipeline
{
public:
    PBRPipeline();
    virtual void Load() override;
    virtual void Update() override;
};

class BackgroundPipeline final : public Pipeline
{
public:
    BackgroundPipeline();
    virtual void Load() override;
    virtual void Update() override;
};

} // namespace trayser