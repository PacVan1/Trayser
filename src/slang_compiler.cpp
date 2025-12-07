#include <pch.h>

#include <slang_compiler.h>
#include <engine.h>
#include <filesystem>

static void Diagnose(Slang::ComPtr<slang::IBlob> blob)
{
    if (blob != nullptr)
    {
        fmt::println("Slang compiler: {}\n", (const char*)blob->getBufferPointer());
    }
}

static VkShaderModule CreateShaderModule(VkDevice device, const Slang::ComPtr<slang::IBlob>& blob)
{
    VkShaderModuleCreateInfo createInfo{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    createInfo.codeSize = blob->getBufferSize();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(blob->getBufferPointer());

    VkShaderModule module;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &module) != VK_SUCCESS)
    {
        fmt::println("Slang compiler: Failed to create shader module.\n");
        return VK_NULL_HANDLE;
    }

    return module;
}

void trayser::SlangCompiler::Init()
{
    SlangResult result{};
    result = slang::createGlobalSession(m_slangGlobalSession.writeRef());
    if (SLANG_FAILED(result))
    {
        fmt::println("Slang compiler: Failed to create global session.\n");
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

VkShaderModule trayser::SlangCompiler::LoadSpirV(const char* fileName)
{
    std::string filePath = FindExistingFile((std::string(fileName) + ".spv").c_str());

    if (filePath.empty())
    {
        return VK_NULL_HANDLE;
    }

    std::ifstream file(filePath, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        return VK_NULL_HANDLE;
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

    VkShaderModule module;
    if (vkCreateShaderModule(g_engine.m_device.m_device, &createInfo, nullptr, &module) != VK_SUCCESS)
    {
        fmt::println("Slang compiler: Failed to create shader module.\n");
        return VK_NULL_HANDLE;
    }

    return module;
}

VkShaderModule trayser::SlangCompiler::Compile(SlangCompileInfo& info)
{
    Slang::ComPtr<slang::ISession> session;
    CreateSlangSession(session);

    // Load the Slang module (source file)
    slang::IModule* slangModule = LoadModule(info.fileName, session);
    if (!slangModule)
        return VK_NULL_HANDLE;

    // Collect module + entry points into a composite
    std::vector<slang::IComponentType*> componentTypes;
    componentTypes.push_back(slangModule);

    for (int i = 0; i < info.entryPointCount; i++)
    {
        Slang::ComPtr<slang::IEntryPoint> entryPoint;
        slangModule->findEntryPointByName(info.entryPointNames[i], entryPoint.writeRef());
        componentTypes.push_back(entryPoint);
    }

    // Compose into a single program
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
            return VK_NULL_HANDLE;
    }

    // Request SPIR-V target code for the whole program
    Slang::ComPtr<slang::IBlob> spirvBlob;
    Slang::ComPtr<slang::IBlob> diagnosticBlob;

    SlangResult result = composedProgram->getTargetCode(0, spirvBlob.writeRef(), diagnosticBlob.writeRef());

    Diagnose(diagnosticBlob);
    if (result == SLANG_FAIL)
        return VK_NULL_HANDLE;

    return CreateShaderModule(g_engine.m_device.m_device, spirvBlob);
}

VkShaderModule trayser::SlangCompiler::CompileAll(const char* fileName)
{
    Slang::ComPtr<slang::ISession> session;
    CreateSlangSession(session);

    slang::IModule* slangModule = LoadModule(fileName, session);
    if (!slangModule)
        return VK_NULL_HANDLE;

    std::vector<slang::IComponentType*> componentTypes;
    componentTypes.push_back(slangModule);

    // Collect all entry points automatically
    int entryPointCount = slangModule->getDefinedEntryPointCount();
    for (int i = 0; i < entryPointCount; i++)
    {
        Slang::ComPtr<slang::IEntryPoint> entryPoint;
        slangModule->getDefinedEntryPoint(i, entryPoint.writeRef());
        componentTypes.push_back(entryPoint);
    }

    // Compose program
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
            return VK_NULL_HANDLE;
    }

    // Emit one SPIR-V blob with all entry points
    Slang::ComPtr<slang::IBlob> spirvBlob;
    Slang::ComPtr<slang::IBlob> diagnosticBlob;
    SlangResult result = composedProgram->getTargetCode(
        0, // target index for SPIR-V
        spirvBlob.writeRef(),
        diagnosticBlob.writeRef());

    Diagnose(diagnosticBlob);
    if (result == SLANG_FAIL)
        return VK_NULL_HANDLE;

    return CreateShaderModule(g_engine.m_device.m_device, spirvBlob);
}

void trayser::SlangCompiler::CreateSlangSession(Slang::ComPtr<slang::ISession>& session) const
{
    slang::TargetDesc targetDesc{};
    targetDesc.format   = SLANG_SPIRV;
    targetDesc.profile  = m_slangGlobalSession->findProfile(kSpirVProfileStr);
    targetDesc.flags    = 0;

    slang::SessionDesc sessionDesc{};
    sessionDesc.targets = &targetDesc;
    sessionDesc.targetCount = 1;
    sessionDesc.compilerOptionEntryCount = 0;
    sessionDesc.searchPaths = m_searchPaths.data();
    sessionDesc.searchPathCount = m_searchPaths.size();

    m_slangGlobalSession->createSession(sessionDesc, session.writeRef());
}

slang::IModule* trayser::SlangCompiler::LoadModule(const char* fileName, Slang::ComPtr<slang::ISession>& session)
{
    slang::IModule* module = nullptr;
    Slang::ComPtr<slang::IBlob> diagnosticBlob;
    module = session->loadModule(fileName, diagnosticBlob.writeRef());
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