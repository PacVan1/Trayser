#include <pch.h>

#include <compiler.h>
#include <engine.h>

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

void trayser::SlangCompiler::Init()
{
    SlangResult result{};
    result = slang::createGlobalSession(m_slangGlobalSession.writeRef());
    if (SLANG_FAILED(result))
    {
        fmt::println("Failed to create Slang global session");
        return;
    }

    m_searchPaths =
    {
        "shaders"
        "../shaders",
        "../../shaders",
        "../../../shaders",
    };
}

bool trayser::SlangCompiler::LoadShaderModule(const char* spirvFileName, VkShaderModule& outModule)
{
    std::string filePath = FindExistingFile((std::string(spirvFileName) + ".spv").c_str());

    if (filePath.empty())
    {
        return false;
    }

    // open the file. With cursor at the end
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        return false;
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
    if (vkCreateShaderModule(g_engine.m_device.m_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
    {
        return false;
    }

    outModule = shaderModule;
    return true;
}

void trayser::SlangCompiler::Compile(SlangCompileInfo& info, VkShaderModule* modules)
{
    Slang::ComPtr<slang::ISession> session;
    CreateSlangSession(session);

    slang::IModule* module = LoadModule(info.fileName, session);
    if (!module)
        return;

    std::vector<slang::IComponentType*> componentTypes;
    componentTypes.reserve(info.entryPointCount + 1);
    componentTypes.push_back(module);

    for (int i = 0; i < info.entryPointCount; i++)
    {
        Slang::ComPtr<slang::IEntryPoint> entryPoint;
        module->findEntryPointByName(info.entryPointNames[i], entryPoint.writeRef());
        componentTypes.push_back(entryPoint);
    }

    Slang::ComPtr<slang::IComponentType> composedProgram;
    {
        Slang::ComPtr<slang::IBlob> diagnosticsBlob;
        SlangResult result = session->createCompositeComponentType(
            componentTypes.data(),
            componentTypes.size(),
            composedProgram.writeRef(),
            diagnosticsBlob.writeRef());

        Diagnose(diagnosticsBlob);
        if (result == SLANG_FAIL)
            return;
    }

    Slang::ComPtr<slang::IBlob> diagnosticBlob;
    std::vector< Slang::ComPtr<slang::IBlob>> spirv;
    spirv.resize(info.entryPointCount);
    for (int i = 0; i < info.entryPointCount; i++)
    {
        composedProgram->getEntryPointCode(i, 0, spirv[i].writeRef(), diagnosticBlob.writeRef());
        Diagnose(diagnosticBlob);
    }

    for (int i = 0; i < info.entryPointCount; i++)
    {
        modules[i] = CreateShaderModule(g_engine.m_device.m_device, spirv[i]);
    }
}

void trayser::SlangCompiler::CreateSlangSession(Slang::ComPtr<slang::ISession>& session) const
{
    slang::TargetDesc targetDesc{};
    targetDesc.format = SLANG_SPIRV;
    targetDesc.profile = m_slangGlobalSession->findProfile("spirv_1_3");
    targetDesc.flags = 0;

    slang::SessionDesc sessionDesc{};
    sessionDesc.targets = &targetDesc;
    sessionDesc.targetCount = 1;
    sessionDesc.compilerOptionEntryCount = 0;
    sessionDesc.searchPaths = m_searchPaths.data();
    sessionDesc.searchPathCount = m_searchPaths.size();

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

std::string trayser::SlangCompiler::FindExistingFile(const char* fileName)
{
    for (auto& path : m_searchPaths)
    {
        std::string fullPath = std::string(path) + "/" + fileName;
        if (std::filesystem::exists(fullPath))
            return fullPath;
    }
    return "";
}