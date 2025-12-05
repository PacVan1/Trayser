#pragma once

#include <vulkan/vulkan.h>

#include <slang/slang.h>
#include <slang/slang-com-ptr.h>

#include <vector>
#include <string>

namespace trayser
{

struct SlangCompileInfo
{
    const char* fileName;
    const char** entryPointNames;
    uint32_t entryPointCount;
};

class SlangCompiler
{
public:
    void Init();
    bool LoadShaderModule(const char* spirvFileName, VkShaderModule& outModule);
    void Compile(SlangCompileInfo& info, VkShaderModule* modules);
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

} // namespace trayser
