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
    const char*     fileName;
    const char**    entryPointNames;
    uint32_t        entryPointCount;
};

class SlangCompiler
{
public:
    void Init();
    /// Creates VkShaderModule of pre-compiled SPIR-V
    VkShaderModule LoadSpirV(const char* fileName);
    /// Composes program of listed entry points and creates VkShaderModule
    VkShaderModule Compile(SlangCompileInfo& info);
    /// Composes program of all entry points and creates VkShaderModule
    VkShaderModule CompileAll(const char* fileName);
    std::string FindExistingFile(const char* fileName);

private:
    void CreateSlangSession(Slang::ComPtr<slang::ISession>& session) const;
    slang::IModule* LoadModule(const char* fileName, Slang::ComPtr<slang::ISession>& session);

public:
    inline static constexpr const char* kSpirVProfileStr = "spirv_1_3";

private:
    Slang::ComPtr<slang::IGlobalSession> m_slangGlobalSession;
    slang::SessionDesc                   m_sessionDesc;
    slang::TargetDesc                    m_targetDesc;
    std::vector<const char*>             m_searchPaths;
};

} // namespace trayser
