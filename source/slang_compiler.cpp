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
        PRINT_ERROR("Slang compiler: Failed to create shader module.");
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
        PRINT_ERROR("Slang compiler: Failed to create global session.");
        abort();
    }

    m_searchPaths =
    {
        "../source",
        "../../source",
        "../../../source",
        "../../../../source",
        "shaders",
        "../shaders",
        "../../shaders",
        "../../../shaders",
    };

    m_targetDesc.format   = SLANG_SPIRV;
    m_targetDesc.profile  = m_slangGlobalSession->findProfile(kSpirVProfileStr);
    m_targetDesc.flags    = 0;

    m_sessionDesc.targets                     = &m_targetDesc;
    m_sessionDesc.targetCount                 = 1;
    m_sessionDesc.compilerOptionEntryCount    = 0;
    m_sessionDesc.searchPaths                 = m_searchPaths.data();
    m_sessionDesc.searchPathCount             = m_searchPaths.size();
    m_sessionDesc.preprocessorMacroCount      = 1;

    m_preprocessorMacros = { "__SLANG__", "1" };
    m_sessionDesc.preprocessorMacros = &m_preprocessorMacros;

    PRINT_INFO("Initialized Slang compiler.");
}

VkShaderModule trayser::SlangCompiler::LoadSpirV(const char* fileName)
{
    std::string filePath = FindExistingFile((std::string(fileName) + ".spv").c_str());

    if (filePath.empty())
    {
        PRINT_WARNING("Slang compiler: Unable to find file.");
        return VK_NULL_HANDLE;
    }

    std::ifstream file(filePath, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        PRINT_WARNING("Slang compiler: Unable to open file.");
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
        PRINT_WARNING("Slang compiler: Failed to create shader module.");
        return VK_NULL_HANDLE;
    }

    PRINT_INFO("Slang compiler: Succesfully loaded SPIR-V.");
    return module;
}

VkShaderModule trayser::SlangCompiler::Compile(SlangCompileInfo& info)
{
    Slang::ComPtr<slang::ISession> session;
    CreateSlangSession(session);

    // Load the Slang module (source file)
    slang::IModule* slangModule = LoadModule(info.fileName, session);
    if (!slangModule)
    {
        PRINT_WARNING("Slang compiler: Unable to create slang module.");
        return VK_NULL_HANDLE;
    }

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
        {
            PRINT_WARNING("Slang compiler: Unable to compose shader program.");
            return VK_NULL_HANDLE;
        }
    }

    // Request SPIR-V target code for the whole program
    Slang::ComPtr<slang::IBlob> spirvBlob;
    Slang::ComPtr<slang::IBlob> diagnosticBlob;

    SlangResult result = composedProgram->getTargetCode(0, spirvBlob.writeRef(), diagnosticBlob.writeRef());

    Diagnose(diagnosticBlob);
    if (result == SLANG_FAIL)
    {
        PRINT_WARNING("Slang compiler: Unable to find target code.");
        return VK_NULL_HANDLE;
    }

    PRINT_INFO("Slang compiler: Succesfully compiled.");
    return CreateShaderModule(g_engine.m_device.m_device, spirvBlob);
}

VkShaderModule trayser::SlangCompiler::CompileAll(const char* fileName)
{
    Slang::ComPtr<slang::ISession> session;
    CreateSlangSession(session);

    slang::IModule* slangModule = LoadModule(fileName, session);
    if (!slangModule)
    {
        PRINT_WARNING("Slang compiler: Unable to create slang module.");
        return VK_NULL_HANDLE;
    }

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
        {
            PRINT_WARNING("Slang compiler: Unable to compose shader program.");
            return VK_NULL_HANDLE;
        }
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
    {
        PRINT_WARNING("Slang compiler: Unable to find target code.");
        return VK_NULL_HANDLE;
    }

    PRINT_INFO("Slang compiler: Succesfully compiled.");
    return CreateShaderModule(g_engine.m_device.m_device, spirvBlob);
}

void trayser::SlangCompiler::CreateSlangSession(Slang::ComPtr<slang::ISession>& session) const
{
    if (m_slangGlobalSession->createSession(m_sessionDesc, session.writeRef()) == SLANG_FAIL)
    {
        PRINT_WARNING("Slang compiler: Unable to create session.");
        abort();
    }
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