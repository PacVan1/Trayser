#include <pch.h>

#include <device.h>
#include <engine.h>
#include <types.h>
#include <images.h>

#include <vk_mem_alloc.h>
#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <imgui_impl_sdl2.h>

const std::vector<const char*> trayser::Device::kValidationLayers = 
{ 
    "VK_LAYER_KHRONOS_validation" 
};

const std::vector<const char*> trayser::Device::kPhysDeviceExtensions = 
{ 
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME
};

static trayser::QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    trayser::QueueFamilyIndices indices{};

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    printf("Queue families found: %u\n", queueFamilyCount);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; i++)
    {
        printf("Queue %u: Flags=0x%x, Count=%u\n", i, queueFamilies[i].queueFlags, queueFamilies[i].queueCount);

        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        printf("Present support: %s\n", presentSupport ? "YES" : "NO");

        if (presentSupport)
        {
            indices.presentFamily = i;
        }

        if (indices.IsComplete()) break;
    }

    return indices;
}

void trayser::Device::ShowCursor(bool show)
{
    SDL_ShowCursor((SDL_bool)!show);
    SDL_SetRelativeMouseMode((SDL_bool)!show);
}

VkResult trayser::Device::CreateBuffer(Buffer& outBuffer, 
    VkDeviceSize                size, 
    VkBufferUsageFlags2KHR      usage, 
    VmaMemoryUsage              memoryUsage, 
    VmaAllocationCreateFlags    flags, 
    std::span<const uint32_t>   queueFamilies)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = queueFamilies.empty() ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT;
    bufferInfo.queueFamilyIndexCount = static_cast<uint32_t>(queueFamilies.size());
    bufferInfo.pQueueFamilyIndices = queueFamilies.data();

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.flags = flags;
    allocInfo.usage = memoryUsage;

    return CreateBuffer(outBuffer, bufferInfo, allocInfo);
}

VkResult trayser::Device::CreateBuffer(Buffer& outBuffer,
    VkDeviceSize              size,
    VkBufferUsageFlags2KHR    usage,
    VkDeviceSize              minAlignment,
    VmaMemoryUsage            memoryUsage,
    VmaAllocationCreateFlags  flags,
    std::span<const uint32_t> queueFamilies)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.pNext = nullptr; // no Flags2 chain
    bufferInfo.size = size;
    bufferInfo.usage =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    bufferInfo.sharingMode = queueFamilies.empty()
        ? VK_SHARING_MODE_EXCLUSIVE
        : VK_SHARING_MODE_CONCURRENT;

    bufferInfo.queueFamilyIndexCount = static_cast<uint32_t>(queueFamilies.size());
    bufferInfo.pQueueFamilyIndices = queueFamilies.empty() ? nullptr : queueFamilies.data();

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.flags = flags;
    allocInfo.usage = memoryUsage;

    return CreateBuffer(outBuffer, bufferInfo, allocInfo, minAlignment);
}

VkResult trayser::Device::CreateBuffer(Buffer& outBuffer, 
    const VkBufferCreateInfo&      bufferInfo, 
    const VmaAllocationCreateInfo& allocInfo, 
    VkDeviceSize                   minAlignment) const
{
    outBuffer = {};

    assert(minAlignment != 0 && (minAlignment & (minAlignment - 1)) == 0);

    VkResult result = vmaCreateBufferWithAlignment(m_allocator, &bufferInfo, &allocInfo, minAlignment,
        &outBuffer.buffer, &outBuffer.allocation, &outBuffer.info);

    if (result != VK_SUCCESS)
    {
        // Handle allocation failure
        printf("Failed to create buffer");
        return result;
    }

    // Get the GPU address of the buffer
    const VkBufferDeviceAddressInfo info = { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = outBuffer.buffer };
    outBuffer.address = vkGetBufferDeviceAddress(m_device, &info);

    return result;
}

VkResult trayser::Device::CreateBuffer(Buffer& outBuffer, 
    const VkBufferCreateInfo& bufferInfo, 
    const VmaAllocationCreateInfo& allocInfo) const
{
    outBuffer = {};

    // Create the buffer
    VmaAllocationInfo allocInfoOut{};

    VkResult result = vmaCreateBuffer(m_allocator, &bufferInfo, &allocInfo,
        &outBuffer.buffer, &outBuffer.allocation, &allocInfoOut);

    if (result != VK_SUCCESS)
    {
        // Handle allocation failure
        printf("Failed to create buffer");
        return result;
    }

    outBuffer.info = allocInfoOut;

    // Get the GPU address of the buffer
    const VkBufferDeviceAddressInfo info = { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = outBuffer.buffer };
    outBuffer.address = vkGetBufferDeviceAddress(m_device, &info);

    return result;
}

VkResult trayser::Device::CreateAccelerationStructure(AccelerationStructure& outAccelStruct,
    const VkAccelerationStructureCreateInfoKHR& createInfo) const
{
    const VmaAllocationCreateInfo allocInfo{ .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE };
    return CreateAccelerationStructure(outAccelStruct, createInfo, allocInfo);
}

VkResult trayser::Device::CreateAccelerationStructure(AccelerationStructure& outAccelStruct,
    const VkAccelerationStructureCreateInfoKHR& createInfo, 
    const VmaAllocationCreateInfo&              allocInfo, 
    std::span<const uint32_t>                   queueFamilies) const
{
    outAccelStruct = {};
    VkAccelerationStructureCreateInfoKHR accelStruct = createInfo;

    // Use legacy usage flags directly
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.pNext = nullptr; // no Flags2 chain
    bufferInfo.size = accelStruct.size;
    bufferInfo.usage =
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    bufferInfo.sharingMode = queueFamilies.empty()
        ? VK_SHARING_MODE_EXCLUSIVE
        : VK_SHARING_MODE_CONCURRENT;
    bufferInfo.queueFamilyIndexCount = static_cast<uint32_t>(queueFamilies.size());
    bufferInfo.pQueueFamilyIndices = queueFamilies.empty() ? nullptr : queueFamilies.data();

    // Step 1: Create the buffer to hold the acceleration structure
    VkResult result = CreateBuffer(outAccelStruct.buffer, bufferInfo, allocInfo);

    if (result != VK_SUCCESS)
    {
        return result;
    }

    // Step 2: Create the acceleration structure with the buffer
    accelStruct.buffer = outAccelStruct.buffer.buffer;
    VK_CHECK(m_rtFuncs.vkCreateAccelerationStructureKHR(m_device, &accelStruct, nullptr, &outAccelStruct.accel));

    {
        VkAccelerationStructureDeviceAddressInfoKHR info{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
                                                         .accelerationStructure = outAccelStruct.accel };
        outAccelStruct.address = m_rtFuncs.vkGetAccelerationStructureDeviceAddressKHR(m_device, &info);
    }

    return result;
}

void trayser::Device::CreateAccelerationStructure(VkAccelerationStructureTypeKHR type, 
    AccelerationStructure&                    outAccelStruct, 
    VkAccelerationStructureGeometryKHR&       geometry, 
    VkAccelerationStructureBuildRangeInfoKHR& buildRangeInfo, 
    VkBuildAccelerationStructureFlagsKHR      flags)
{
    VkDevice device = m_device;

    // Helper function to align a value to a given alignment
    auto alignUp = [](auto value, size_t alignment) noexcept { return ((value + alignment - 1) & ~(alignment - 1)); };

    // Fill the build information with the current information, the rest is filled later (scratch buffer and destination AS)
    VkAccelerationStructureBuildGeometryInfoKHR asBuildInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = type,  // The type of acceleration structure (BLAS or TLAS)
        .flags = flags,   // Build flags (e.g. prefer fast trace)
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,  // Build mode vs update
        .geometryCount = 1,                                               // Deal with one geometry at a time
        .pGeometries = &geometry,  // The geometry to build the acceleration structure from
    };

    // One geometry at a time (could be multiple)
    std::vector<uint32_t> maxPrimCount(1);
    maxPrimCount[0] = buildRangeInfo.primitiveCount;

    // Find the size of the acceleration structure and the scratch buffer
    VkAccelerationStructureBuildSizesInfoKHR asBuildSize{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    m_rtFuncs.vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &asBuildInfo,
        maxPrimCount.data(), &asBuildSize);

    // Make sure the scratch buffer is properly aligned
    VkDeviceSize scratchSize = alignUp(asBuildSize.buildScratchSize, m_asProperties.minAccelerationStructureScratchOffsetAlignment);

    // Create the scratch buffer to store the temporary data for the build
    Buffer scratchBuffer;
    VK_CHECK(CreateBuffer(scratchBuffer, scratchSize,
        VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT
        | VK_BUFFER_USAGE_2_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR, m_asProperties.minAccelerationStructureScratchOffsetAlignment));

    // Create the acceleration structure
    VkAccelerationStructureCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .size = asBuildSize.accelerationStructureSize,  // The size of the acceleration structure
        .type = type,  // The type of acceleration structure (BLAS or TLAS)
    };
    VK_CHECK(CreateAccelerationStructure(outAccelStruct, createInfo));

    // Build the acceleration structure
    {
        VkCommandBuffer cmd;
        BeginOneTimeSubmit(cmd);

        // Fill with new information for the build,scratch buffer and destination AS
        asBuildInfo.dstAccelerationStructure = outAccelStruct.accel;
        asBuildInfo.scratchData.deviceAddress = scratchBuffer.address;

        VkAccelerationStructureBuildRangeInfoKHR* pBuildRangeInfo = &buildRangeInfo;
        m_rtFuncs.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &asBuildInfo, &pBuildRangeInfo);

        EndOneTimeSubmit();
    }
    // Cleanup the scratch buffer
    //m_allocator.destroyBuffer(scratchBuffer);
}

void trayser::Device::CreateAccelerationStructure2(VkAccelerationStructureTypeKHR type, 
    AccelerationStructure& outAccelStruct, 
    std::vector<VkAccelerationStructureGeometryKHR>& geometries, 
    std::vector<VkAccelerationStructureBuildRangeInfoKHR>& rangeInfos, 
    VkBuildAccelerationStructureFlagsKHR flags)
{
    // Helper function to align a value to a given alignment
    auto alignUp = [](auto value, size_t alignment) noexcept { return ((value + alignment - 1) & ~(alignment - 1)); };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = type;
    buildInfo.flags = flags;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.geometryCount = (uint32_t)geometries.size();
    buildInfo.pGeometries = geometries.data();

    std::vector<uint32_t> maxPrimCount(rangeInfos.size());
    for (int i = 0; i < maxPrimCount.size(); i++)
    {
        maxPrimCount[i] = rangeInfos[i].primitiveCount;
    }

    VkAccelerationStructureBuildSizesInfoKHR buildSize{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    m_rtFuncs.vkGetAccelerationStructureBuildSizesKHR(m_device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, maxPrimCount.data(), &buildSize);

    // Make sure the scratch buffer is properly aligned
    VkDeviceSize scratchSize = alignUp(buildSize.buildScratchSize, m_asProperties.minAccelerationStructureScratchOffsetAlignment);

    // Create the scratch buffer to store the temporary data for the build
    Buffer scratchBuffer;
    VK_CHECK(CreateBuffer(scratchBuffer, scratchSize,
        VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT
        | VK_BUFFER_USAGE_2_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR, m_asProperties.minAccelerationStructureScratchOffsetAlignment));

    // Create the acceleration structure
    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.size = buildSize.accelerationStructureSize;  // The size of the acceleration structure
    createInfo.type = type;  // The type of acceleration structure (BLAS or TLAS)
    VK_CHECK(CreateAccelerationStructure(outAccelStruct, createInfo));

    VkCommandBuffer cmd;
    BeginOneTimeSubmit(cmd);

    // Fill with new information for the build,scratch buffer and destination AS
    buildInfo.dstAccelerationStructure = outAccelStruct.accel;
    buildInfo.scratchData.deviceAddress = scratchBuffer.address;

    VkAccelerationStructureBuildRangeInfoKHR* pBuildRangeInfo = rangeInfos.data();
    m_rtFuncs.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pBuildRangeInfo);

    EndOneTimeSubmit();

    // Cleanup the scratch buffer
    vmaDestroyBuffer(m_allocator, scratchBuffer.buffer, scratchBuffer.allocation);
}

void trayser::Device::PrimitiveToGeometry(const Mesh& mesh,
    VkAccelerationStructureGeometryKHR& geometry,
    VkAccelerationStructureBuildRangeInfoKHR& rangeInfo)
{
    VkAccelerationStructureGeometryTrianglesDataKHR triangles{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,  // vec3 vertex position data
        .vertexData = {.deviceAddress = mesh.vertexBufferAddr + offsetof(Vertex, position) },
        .vertexStride = sizeof(Vertex),
        .maxVertex = mesh.vertexCount - 1,
        .indexType = VK_INDEX_TYPE_UINT32,  // Index type (VK_INDEX_TYPE_UINT16 or VK_INDEX_TYPE_UINT32)
        .indexData = {.deviceAddress = mesh.indexBufferAddr },
    };

    // Identify the above data as containing opaque triangles.
    geometry = VkAccelerationStructureGeometryKHR{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry = {.triangles = triangles},
        .flags = VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR | VK_GEOMETRY_OPAQUE_BIT_KHR,
    };

    rangeInfo = VkAccelerationStructureBuildRangeInfoKHR
    { 
        .primitiveCount = mesh.indexCount / 3,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0
    };
}

void trayser::Device::PrimitivesToGeometries(const Mesh& mesh, 
    std::vector<VkAccelerationStructureGeometryKHR>& geometries, 
    std::vector<VkAccelerationStructureBuildRangeInfoKHR>& rangeInfos)
{
    geometries.clear();
    rangeInfos.clear();
    geometries.reserve(mesh.primitives.size());
    rangeInfos.reserve(mesh.primitives.size());

    for (auto& prim : mesh.primitives)
    {
        auto& geometry = geometries.emplace_back();
        auto& rangeInfo = rangeInfos.emplace_back();

        VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
        triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        triangles.vertexData = { .deviceAddress = mesh.vertexBufferAddr + offsetof(Vertex, position) };
        triangles.vertexStride = sizeof(Vertex);
        triangles.maxVertex = mesh.vertexCount - 1;
        triangles.indexType = VK_INDEX_TYPE_UINT32;
        triangles.indexData = { .deviceAddress = mesh.indexBufferAddr };

        geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        geometry.flags = VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR | VK_GEOMETRY_OPAQUE_BIT_KHR;
        geometry.geometry.triangles = triangles;

        rangeInfo.primitiveCount = prim.indexCount / 3;
        rangeInfo.primitiveOffset = prim.baseIndex * sizeof(uint32_t);
        rangeInfo.firstVertex = prim.baseVertex;
        rangeInfo.transformOffset = 0;
    }
}

void trayser::Device::BeginOneTimeSubmit(VkCommandBuffer& outCmd) const
{
    VK_CHECK(vkResetFences(m_device, 1, &m_oneTimeFence));
    VK_CHECK(vkResetCommandBuffer(m_oneTimeCommandBuffer, 0));

    VkCommandBufferBeginInfo cmdBeginInfo = CommandBufferBeginInfo();
    cmdBeginInfo.pInheritanceInfo = nullptr;
    cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(m_oneTimeCommandBuffer, &cmdBeginInfo));

    outCmd = m_oneTimeCommandBuffer;
}

void trayser::Device::EndOneTimeSubmit() const
{
    VK_CHECK(vkEndCommandBuffer(m_oneTimeCommandBuffer));

    VkCommandBufferSubmitInfo cmdSubmitInfo = CommandBufferSubmitInfo();
	cmdSubmitInfo.commandBuffer = m_oneTimeCommandBuffer;
    cmdSubmitInfo.deviceMask = 0;

    VkSubmitInfo2 submitInfo            = SubmitInfo2();
	submitInfo.pSignalSemaphoreInfos    = nullptr;
	submitInfo.signalSemaphoreInfoCount = 0;
	submitInfo.pWaitSemaphoreInfos      = nullptr;
	submitInfo.waitSemaphoreInfoCount   = 0;
	submitInfo.pCommandBufferInfos      = &cmdSubmitInfo;
	submitInfo.commandBufferInfoCount   = 1;

    VK_CHECK(vkQueueSubmit2(m_graphicsQueue, 1, &submitInfo, m_oneTimeFence));
    VK_CHECK(vkWaitForFences(m_device, 1, &m_oneTimeFence, true, 9999999999999));
}

void trayser::Device::BeginFrame()
{
    ProcessSDLEvents();

    VK_CHECK(vkWaitForFences(m_device, 1, &m_swapchain.GetFence(), true, 1000000000));
    // Destroy resources (descriptors, etc)
    VK_CHECK(vkResetFences(m_device, 1, &m_swapchain.GetFence()));

    uint32_t swapchainIdx;
    VkResult e = vkAcquireNextImageKHR(m_device, m_swapchain.m_swapchain, 1000000000, m_swapchain.GetFrame().swapchainSemaphore, nullptr, &swapchainIdx);
    if (e == VK_ERROR_OUT_OF_DATE_KHR)
    {
        m_windowResized = true;
    }

    if (m_windowResized)
        //ResizeSwapchain();

    // imgui new frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void trayser::Device::EndFrame()
{
    // set swapchain image layout to Attachment Optimal so we can draw it
    vkutil::TransitionImage(GetCmd(), m_swapchain.GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    //draw imgui into the swapchain image
    RenderImGui();

    // set swapchain image layout to Present so we can draw it
    vkutil::TransitionImage(GetCmd(), m_swapchain.GetImage(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VK_CHECK(vkEndCommandBuffer(GetCmd()));

    // Prepare the submission to the queue. 
    // We want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
    // We will signal the _renderSemaphore, to signal that rendering has finished
    VkCommandBufferSubmitInfo cmdSubmitInfo = CommandBufferSubmitInfo();
	cmdSubmitInfo.commandBuffer = GetCmd();
    cmdSubmitInfo.deviceMask    = 0;

    VkSemaphoreSubmitInfo waitInfo = SemaphoreSubmitInfo();
	waitInfo.semaphore = m_swapchain.GetFrame().swapchainSemaphore;
    waitInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
    waitInfo.deviceIndex = 0;
    waitInfo.value = 1;

    VkSemaphoreSubmitInfo signalInfo = SemaphoreSubmitInfo();
    signalInfo.semaphore = m_swapchain.GetFrame().renderSemaphore;
    signalInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
    signalInfo.deviceIndex = 0;
    signalInfo.value = 1;

    VkSubmitInfo2 submitInfo            = SubmitInfo2();
    submitInfo.pWaitSemaphoreInfos      = &waitInfo;
    submitInfo.waitSemaphoreInfoCount   = 1;
	submitInfo.pSignalSemaphoreInfos    = &signalInfo;
    submitInfo.signalSemaphoreInfoCount = 1;
    submitInfo.pCommandBufferInfos      = &cmdSubmitInfo;
	submitInfo.commandBufferInfoCount   = 1;

    // Submit command buffer to the queue and execute it.
    // _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(m_graphicsQueue, 1, &submitInfo, m_swapchain.GetFence()));

    // Prepare present
    // This will put the image we just rendered to into the visible window.
    // We want to wait on the _renderSemaphore for that, 
    // as its necessary that drawing commands have finished before the image is displayed to the user
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.pSwapchains = &m_swapchain.m_swapchain;
    presentInfo.swapchainCount = 1;
    presentInfo.pWaitSemaphores = &m_swapchain.GetFrame().renderSemaphore;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pImageIndices = &m_swapchain.m_imageIdx;

    VkResult presentResult = vkQueuePresentKHR(m_graphicsQueue, &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        m_windowResized = true;
    }

    m_swapchain.m_frameIdx = (m_swapchain.m_frameIdx + 1) % kFrameCount;

    m_input.UpdatePrevious();
}

bool trayser::Device::ShouldQuit() const
{
    return m_quit;
}

void trayser::Device::CreateBottomLevelAs()
{
    m_blasAccel.resize(128);

    // One BLAS per primitive
    for (int i = 0; i < g_engine.m_meshPool.m_resources.size(); i++)
    {
        if (!g_engine.m_meshPool.m_takenSpots[i])
            continue;

        const Mesh& mesh = g_engine.m_meshPool.Get(i);

        VkAccelerationStructureGeometryKHR       asGeometry{};
        VkAccelerationStructureBuildRangeInfoKHR asBuildRangeInfo{};

        // Convert the primitive information to acceleration structure geometry
        PrimitiveToGeometry(mesh, asGeometry, asBuildRangeInfo);

        m_blasAccel.emplace_back();
        CreateAccelerationStructure(VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, m_blasAccel[i], asGeometry,
            asBuildRangeInfo, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
    }
}

void trayser::Device::CreateBLas2()
{
    m_blasAccel.resize(128);

    for (int i = 0; i < g_engine.m_meshPool.m_resources.size(); i++)
    {
        if (!g_engine.m_meshPool.m_takenSpots[i])
            continue;

        const Mesh& mesh = g_engine.m_meshPool.Get(i);
        std::vector<VkAccelerationStructureGeometryKHR> geometries;
        std::vector<VkAccelerationStructureBuildRangeInfoKHR> rangeInfos;

        PrimitivesToGeometries(mesh, geometries, rangeInfos);

        CreateAccelerationStructure2(VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, m_blasAccel[i], geometries,
            rangeInfos, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
    }
}

void trayser::Device::CreateTopLevelAs()
{
    // VkTransformMatrixKHR is row-major 3x4, glm::mat4 is column-major; transpose before memcpy.
    auto toTransformMatrixKHR = [](const glm::mat4& m) {
        VkTransformMatrixKHR t;
        memcpy(&t, glm::value_ptr(glm::transpose(m)), sizeof(t));
        return t;
        };

    // First create the instance data for the TLAS
    std::vector<VkAccelerationStructureInstanceKHR> tlasInstances;
    auto view = g_engine.m_scene.m_registry.view<WorldTransform, RenderComponent>();
    int i = 0;
    for (const auto& [entity, transform, render] : view.each())
    {
        VkAccelerationStructureInstanceKHR asInstance{};
        asInstance.transform = toTransformMatrixKHR(transform.matrix);  // Position of the instance
        asInstance.instanceCustomIndex = render.mesh;                       // gl_InstanceCustomIndexEXT
        asInstance.accelerationStructureReference = m_blasAccel[render.mesh].address;  // Address of the BLAS
        asInstance.instanceShaderBindingTableRecordOffset = 0;  // We will use the same hit group for all objects
        asInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_CULL_DISABLE_BIT_NV;  // No culling - double sided
        asInstance.mask = 0xFF;
        tlasInstances.emplace_back(asInstance);
        i++;
    }

    // Then create the buffer with the instance data
    Buffer tlasInstancesBuffer;
    {
        //CreateBuffer(
        //    tlasInstancesBuffer, std::span<VkAccelerationStructureInstanceKHR const>(tlasInstances).size_bytes(),
        //    VK_BUFFER_USAGE_2_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

        //VkCommandBuffer cmd;
		//BeginOneTimeSubmit(cmd);
        //StageBuffer(tlasInstancesBuffer, tlasInstances.data(), tlasInstancesBuffer.info.size);

        // 1) Create destination TLAS instance buffer (device-local, not mapped)
        VkBufferCreateInfo dstInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        dstInfo.size = std::span<VkAccelerationStructureInstanceKHR const>(tlasInstances).size_bytes(); // sizeof(VkAccelerationStructureInstanceKHR) * count
        dstInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        VmaAllocationCreateInfo dstAlloc{};
        dstAlloc.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        //dstAlloc.flags = VMA_ALLOCATION_CREATE_BUFFER_DEVICE_ADDRESS_BIT; // for device address

        VmaAllocationInfo dstAllocInfo;
        vmaCreateBuffer(g_engine.m_device.m_allocator, &dstInfo, &dstAlloc,
            &tlasInstancesBuffer.buffer, &tlasInstancesBuffer.allocation, &dstAllocInfo);

        // 2) Create staging buffer (host-visible, mapped or map manually)
        VkBufferCreateInfo stagingInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        stagingInfo.size = dstInfo.size;
        stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        VmaAllocationCreateInfo stagingAlloc{};
        stagingAlloc.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        stagingAlloc.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT; // optional

        Buffer staging;
        VmaAllocationInfo stagingInfoOut;
        vmaCreateBuffer(g_engine.m_device.m_allocator, &stagingInfo, &stagingAlloc,
            &staging.buffer, &staging.allocation, &stagingInfoOut);

        // 3) Write instance data into staging
        void* mapped = stagingInfoOut.pMappedData;
        if (!mapped) {
            vmaMapMemory(g_engine.m_device.m_allocator, staging.allocation, &mapped);
        }
        memcpy(mapped, tlasInstances.data(), dstInfo.size);
        if (stagingInfoOut.pMappedData == nullptr) {
            vmaUnmapMemory(g_engine.m_device.m_allocator, staging.allocation);
        }

        // 4) Copy staging -> destination
        VkCommandBuffer cmd;
        g_engine.m_device.BeginOneTimeSubmit(cmd);

        VkBufferCopy region{ };
        region.srcOffset = 0;
        region.dstOffset = 0;
        region.size = dstInfo.size;
        vkCmdCopyBuffer(cmd, staging.buffer, tlasInstancesBuffer.buffer, 1, &region);

        g_engine.m_device.EndOneTimeSubmit(); // ensure it waits for completion

        VkBufferDeviceAddressInfo addrInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
        addrInfo.buffer = tlasInstancesBuffer.buffer;
        tlasInstancesBuffer.address = vkGetBufferDeviceAddress(g_engine.m_device.m_device, &addrInfo);

        // 5) Destroy staging buffer
        vmaDestroyBuffer(g_engine.m_device.m_allocator, staging.buffer, staging.allocation);

        //EndOneTimeSubmit();
    }

    // Then create the TLAS geometry
    {
        VkAccelerationStructureGeometryKHR       asGeometry{};
        VkAccelerationStructureBuildRangeInfoKHR asBuildRangeInfo{};

        // Convert the instance information to acceleration structure geometry, similar to primitiveToGeometry()
        VkAccelerationStructureGeometryInstancesDataKHR geometryInstances
        {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
            .pNext = nullptr,
            .arrayOfPointers = VK_FALSE,
            .data = {.deviceAddress = tlasInstancesBuffer.address}
        };
        asGeometry = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
                            .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
                            .geometry = {.instances = geometryInstances} };
        asBuildRangeInfo = { .primitiveCount = static_cast<uint32_t>(i) };

        CreateAccelerationStructure(VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, m_tlasAccel, asGeometry,
            asBuildRangeInfo, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
    }

    //m_allocator.destroyBuffer(tlasInstancesBuffer);  // Cleanup
}

void trayser::Device::Init()
{
	InitSDL();
	InitInstance();
	InitSurface();
	InitDebugMessenger();
	InitPhysicalDevice();
	InitLogicalDevice();
    m_swapchain.Init(*this);
    InitVMA();
    InitCommands();
    InitSyncStructures();
    InitImGui();
    InitRayTracing();
    m_rtFuncs.Init();

    PRINT_INFO("Initialized device.");
}

void trayser::Device::Destroy()
{
    DestroyImGui();
    DestroySyncStructures();
    DestroyCommands();
    DestroyVMA();
    m_swapchain.Destroy(*this);
    DestroyLogicalDevice();
    DestroyDebugMessenger();
    DestroySurface();
    DestroyInstance();
    DestroySDL();
}

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    switch (messageSeverity)
    {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: PRINT_INFO(pCallbackData->pMessage); break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: PRINT_WARNING(pCallbackData->pMessage); break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: PRINT_ERROR(pCallbackData->pMessage); break;
    }
    return VK_FALSE;
}

void trayser::Device::InitSDL()
{
    if (SDL_Init(SDL_INIT_VIDEO) != SDL_FALSE)
    {
        PRINT_ERROR("Unable to initialize SDL.");
        abort();
    }

    m_window = SDL_CreateWindow(
        kEngineName,
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        kInitWindowWidth,
        kInitWindowHeight,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

    if (!m_window)
    {
        PRINT_ERROR("Unable to create SDL window.");
        abort();
    }

    PRINT_INFO("Initialized SDL.");
}

void trayser::Device::InitInstance()
{
    if (kUseValidationLayers && !CheckValidationLayerSupport())
    {
        throw std::runtime_error("Validation layers requested, but not available!");
    }

    // Technically optional, but may provide useful info to the driver
    VkApplicationInfo appInfo{};
    appInfo.sType               = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName    = kEngineName;
    appInfo.applicationVersion  = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName         = kEngineName;
    appInfo.engineVersion       = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion          = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    // Gather available extensions and print
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

    // Tell vk what extensions to use and how many
    auto glfwExtensions = GetRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(glfwExtensions.size());
    createInfo.ppEnabledExtensionNames = glfwExtensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (kUseValidationLayers)
    {
        createInfo.enabledLayerCount = static_cast<uint32_t>(kValidationLayers.size());
        createInfo.ppEnabledLayerNames = kValidationLayers.data();

        PopulateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
    }
    else
    {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }

    if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Vulkan instance!");
    }

    PRINT_INFO("Initialized Vulkan instance.");
}

static VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else
    {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void trayser::Device::InitDebugMessenger()
{
    if (!kUseValidationLayers)
        return;

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = DebugCallback;

    if (CreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to set up Vulkan debug messenger!");
    }

    PRINT_INFO("Initialized Vulkan validation layers.");
}

void trayser::Device::InitPhysicalDevice()
{
    PRINT_INFO("Initializig Vulkan physical device.");

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);

    if (deviceCount == 0)
    {
        PRINT_ERROR("Unable to find GPUs with Vulkan support.");
        abort();
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

    for (uint32_t i = 0; i < deviceCount; i++) 
    {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(devices[i], &props);

        std::cout << "Device " << i << ": " << props.deviceName << std::endl;
        std::cout << "Type: ";
        switch (props.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: std::cout << "Integrated GPU"; break;
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   std::cout << "Discrete GPU"; break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    std::cout << "Virtual GPU"; break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:            std::cout << "CPU"; break;
        default:                                     std::cout << "Other"; break;
        }
        std::cout << std::endl;
    }

    for (const auto& device : devices)
    {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(device, &props);
        printf("Checking device: %s (%u)\n", props.deviceName, props.deviceType);

        if (IsPhysicalDeviceSuitable(device))
        {
            printf("Device is suitable: %s\n", props.deviceName);
            m_physDevice = device;
            break;
        }
        else
        {
            printf("Device not suitable: %s\n", props.deviceName);
        }
    }

    if (m_physDevice == VK_NULL_HANDLE)
    {
        PRINT_ERROR("Unable to find suitable device.");
        abort();
    }

    PRINT_INFO("Initialized Vulkan physical device.");
}

void trayser::Device::InitLogicalDevice()
{
    QueueFamilyIndices indices = FindQueueFamilies(m_physDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    // Vulkan lets you assign priorities to queues to influence the scheduling of 
    // command buffer execution using floating point numbers between 0.0 and 1.0. 
    // This is required even if there is only a single queue:
    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies)
    {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    // Core features
    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;

    // Extension features
    VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures{};
    bufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
    bufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeatures{};
    accelFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    accelFeatures.accelerationStructure = VK_TRUE;
    accelFeatures.pNext = &bufferDeviceAddressFeatures;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures{};
    rtPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rtPipelineFeatures.rayTracingPipeline = VK_TRUE;
    rtPipelineFeatures.pNext = &accelFeatures;

    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering = VK_TRUE;      // required for pipelines without renderPass
    features13.synchronization2 = VK_TRUE;      // you use vkCmdPipelineBarrier2 / vkQueueSubmit2
    features13.pNext = &rtPipelineFeatures;     // chain (plus your ray tracing feature structs)

    // Device create info
    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pEnabledFeatures = &deviceFeatures; // core features
    createInfo.enabledExtensionCount = static_cast<uint32_t>(kPhysDeviceExtensions.size());
    createInfo.ppEnabledExtensionNames = kPhysDeviceExtensions.data();
    createInfo.pNext = &features13; // chain extension features

    if (kUseValidationLayers)
    {
        createInfo.enabledLayerCount = static_cast<uint32_t>(kValidationLayers.size());
        createInfo.ppEnabledLayerNames = kValidationLayers.data();
    }
    else
    {
        createInfo.enabledLayerCount = 0;
    }

    VK_CHECK(vkCreateDevice(m_physDevice, &createInfo, nullptr, &m_device));

    // Get handle to graphics queue
    vkGetDeviceQueue(m_device, indices.graphicsFamily.value(), 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, indices.presentFamily.value(), 0, &m_presentQueue);

    PRINT_INFO("Initialized Vulkan logical device.");
}

void trayser::Device::InitSurface()
{
    if (SDL_Vulkan_CreateSurface(m_window, m_instance, &m_surface) == SDL_FALSE)
    {
        PRINT_ERROR("Unable to create Vulkan surface.");
        abort();
    }
    PRINT_INFO("Initialized Vulkan surface.");
}

VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
    for (const auto& availableFormat : availableFormats)
    {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
    for (const auto& availablePresentMode : availablePresentModes)
    {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return availablePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D trayser::Device::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return capabilities.currentExtent;
    }
    else
    {
        int width, height;
        SDL_GetWindowSize(m_window, &width, &height);

        VkExtent2D actualExtent =
        {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

void trayser::Device::InitCommands()
{
    VkCommandPoolCreateInfo poolCreateInfo = CommandPoolCreateInfo();
    poolCreateInfo.queueFamilyIndex = m_graphicsQueueFamily;
    poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VK_CHECK(vkCreateCommandPool(m_device, &poolCreateInfo, nullptr, &m_oneTimeCommandPool));

    VkCommandBufferAllocateInfo cmdAllocInfo = CommandBufferAllocateInfo();
    cmdAllocInfo.commandPool = m_oneTimeCommandPool;
    cmdAllocInfo.commandBufferCount = 1;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    VK_CHECK(vkAllocateCommandBuffers(m_device, &cmdAllocInfo, &m_oneTimeCommandBuffer));
}

void trayser::Device::InitSyncStructures()
{
    VkFenceCreateInfo createInfo = FenceCreateInfo();
    createInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VK_CHECK(vkCreateFence(m_device, &createInfo, nullptr, &m_oneTimeFence));
}

void trayser::Device::InitVMA()
{
    // Initialize VMA
    VmaAllocatorCreateInfo info = {};
    info.physicalDevice = m_physDevice;
    info.device         = m_device;
    info.instance       = m_instance;
    info.flags          = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    VK_CHECK(vmaCreateAllocator(&info, &m_allocator));

    PRINT_INFO("Initialized VMA.");
}

void trayser::Device::InitImGui()
{
    VkDescriptorPoolSize poolSizes[] = 
    { 
        { VK_DESCRIPTOR_TYPE_SAMPLER,                   1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,    1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,             1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,             1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,      1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,      1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,            1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,            1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,    1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,    1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,          1000 } 
    };

    VkDescriptorPoolCreateInfo poolInfo = 
    {
    .sType          = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .flags          = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
    .maxSets        = 1000,
    .poolSizeCount  = (u32)std::size(poolSizes),
    .pPoolSizes     = poolSizes 
    };

    VkDescriptorPool pool;
    VK_CHECK(vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &pool));

    ImGui_ImplVulkan_InitInfo initInfo = 
    {
    .Instance              = m_instance,
    .PhysicalDevice        = m_physDevice,
    .Device                = m_device,
    .Queue                 = m_graphicsQueue,
    .DescriptorPool        = pool,
    .MinImageCount         = 3,
    .ImageCount            = 3,
    .UseDynamicRendering   = true
    };

    initInfo.PipelineRenderingCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    initInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    initInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = &m_swapchain.m_format;
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForVulkan(m_window);
    ImGui_ImplVulkan_Init(&initInfo);
    ImGui_ImplVulkan_CreateFontsTexture();

    PRINT_INFO("Initialized ImGui.");
}

void trayser::Device::DestroySDL() const
{
    SDL_DestroyWindow(m_window);
    SDL_Quit();
}

void trayser::Device::DestroyInstance() const
{
    vkDestroyInstance(m_instance, nullptr);
}

void trayser::Device::DestroySurface() const
{
    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
}

void trayser::Device::DestroyDebugMessenger() const
{
    if (!kUseValidationLayers)
        return;

    auto function = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT");
    if (function != nullptr)
    {
        function(m_instance, m_debugMessenger, nullptr);
    }
}

void trayser::Device::DestroyLogicalDevice() const
{
    vkDestroyDevice(m_device, nullptr);
}

void trayser::Device::DestroyVMA() const
{
    vmaDestroyAllocator(m_allocator);
}

void trayser::Device::DestroyImGui() const
{
    ImGui_ImplVulkan_Shutdown();
    ImGui::DestroyContext();
}

void trayser::Device::DestroyCommands() const
{
    vkDestroyCommandPool(m_device, m_oneTimeCommandPool, nullptr);
}

void trayser::Device::DestroySyncStructures() const
{
    vkDestroyFence(m_device, m_oneTimeFence, nullptr);
}

void trayser::Device::InitRayTracing()
{
    m_rtProperties = VkPhysicalDeviceRayTracingPipelinePropertiesKHR{};
    m_rtProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

    m_asProperties = VkPhysicalDeviceAccelerationStructurePropertiesKHR{};
    m_asProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;

    VkPhysicalDeviceProperties2 properties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
    properties.pNext = &m_rtProperties;
    m_rtProperties.pNext = &m_asProperties;
    vkGetPhysicalDeviceProperties2(m_physDevice, &properties);
}

void trayser::Device::RenderImGui() const
{
    ImGui::Render();
    VkRenderingAttachmentInfo colorAttachment = RenderingAttachmentInfo();
    colorAttachment.imageView   = m_swapchain.GetImageView();
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp      = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo renderInfo = RenderingInfo();
    renderInfo.renderArea = VkRect2D{ VkOffset2D { 0, 0 }, m_swapchain.m_extent };
    renderInfo.pColorAttachments = &colorAttachment;
    renderInfo.pStencilAttachment = nullptr;
    renderInfo.pDepthAttachment = nullptr;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.layerCount = 1;
    
    vkCmdBeginRendering(GetCmd(), &renderInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), GetCmd());
    vkCmdEndRendering(GetCmd());
}

void trayser::Device::ProcessSDLEvents()
{
    m_input.m_mouseDeltaPos = { 0.0f, 0.0f };
    m_input.m_scroll = 0.0f;

    // Handle events on queue
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0)
    {
        // Close the window when user alt-f4s or clicks the X button
        if (e.type == SDL_QUIT)
            m_quit = true;

        if (e.type == SDL_WINDOWEVENT)
        {
            if (e.window.event == SDL_WINDOWEVENT_RESIZED)
            {
                //ResizeSwapchain();
            }
        }

        m_input.ProcessEvent(e);
        ImGui_ImplSDL2_ProcessEvent(&e);
    }
}

bool trayser::Device::CheckValidationLayerSupport()
{
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    // Check if all g_vkValidationLayers exist
    for (const char* layerName : kValidationLayers)
    {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers)
        {
            if (strcmp(layerName, layerProperties.layerName) == 0)
            {
                layerFound = true;
                break;
            }
        }

        if (!layerFound)
        {
            return false;
        }
    }

    return true;
}

std::vector<const char*> trayser::Device::GetRequiredExtensions()
{
    unsigned int count = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(m_window, &count, nullptr))
    {
        throw std::runtime_error("SDL_Vulkan_GetInstanceExtensions failed to get count");
    }

    std::vector<const char*> extensions(count);
    if (!SDL_Vulkan_GetInstanceExtensions(m_window, &count, extensions.data()))
    {
        throw std::runtime_error("SDL_Vulkan_GetInstanceExtensions failed to get names");
    }

    if (kUseValidationLayers)
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

void trayser::Device::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = DebugCallback;
}

trayser::QueueFamilyIndices trayser::Device::FindQueueFamilies(VkPhysicalDevice device)
{
    QueueFamilyIndices indices{};

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    printf("Queue families found: %u\n", queueFamilyCount);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; i++)
    {
        printf("Queue %u: Flags=0x%x, Count=%u\n", i, queueFamilies[i].queueFlags, queueFamilies[i].queueCount);

        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);
        printf("Present support: %s\n", presentSupport ? "YES" : "NO");

        if (presentSupport)
        {
            indices.presentFamily = i;
        }

        if (indices.IsComplete()) break;
    }

    return indices;
}

bool trayser::Device::IsPhysicalDeviceSuitable(VkPhysicalDevice device)
{
    QueueFamilyIndices indices = FindQueueFamilies(device);

    printf("Graphics queue: %s\n", indices.graphicsFamily.has_value() ? "FOUND" : "MISSING");
    printf("Present queue: %s\n", indices.presentFamily.has_value() ? "FOUND" : "MISSING");

    bool extensionsSupported = CheckPhysicalDeviceExtensionSupport(device);
    printf("Extensions supported: %s\n", extensionsSupported ? "YES" : "NO");

    bool swapChainAdequate = false;
    if (extensionsSupported)
    {
        SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();

        printf("Swapchain formats count: %zu\n", swapChainSupport.formats.size());
        printf("Swapchain present modes count: %zu\n", swapChainSupport.presentModes.size());
        printf("Swapchain adequate: %s\n", swapChainAdequate ? "YES" : "NO");
    }

    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);
    printf("Sampler anisotropy: %s\n", supportedFeatures.samplerAnisotropy ? "YES" : "NO");

    bool suitable = indices.IsComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
    printf("Device suitable: %s\n", suitable ? "YES" : "NO");

    return suitable;
}

bool trayser::Device::CheckPhysicalDeviceExtensionSupport(VkPhysicalDevice device)
{
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(kPhysDeviceExtensions.begin(), kPhysDeviceExtensions.end());

    printf("Available extensions (%u):\n", extensionCount);
    for (const auto& extension : availableExtensions)
    {
        printf("  %s\n", extension.extensionName);
        requiredExtensions.erase(extension.extensionName);
    }

    if (!requiredExtensions.empty())
    {
        printf("Missing required extensions:\n");
        for (const auto& ext : requiredExtensions)
            printf("  %s\n", ext.c_str());
    }

    return requiredExtensions.empty();
}

trayser::SwapChainSupportDetails trayser::Device::QuerySwapChainSupport(VkPhysicalDevice device) const
{
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);
    printf("Surface capabilities: minImageCount=%u, maxImageCount=%u\n",
           details.capabilities.minImageCount, details.capabilities.maxImageCount);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);

    if (formatCount != 0)
    {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, details.formats.data());
    }
    printf("Surface formats count: %u\n", formatCount);

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr);

    if (presentModeCount != 0)
    {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, details.presentModes.data());
    }
    printf("Present modes count: %u\n", presentModeCount);

    return details;
}

void trayser::RuntimeFuncs::Init()
{
    vkCreateAccelerationStructureKHR =
        (PFN_vkCreateAccelerationStructureKHR)vkGetDeviceProcAddr(g_engine.m_device.m_device, "vkCreateAccelerationStructureKHR"); 

    vkCmdBuildAccelerationStructuresKHR =
        (PFN_vkCmdBuildAccelerationStructuresKHR)vkGetDeviceProcAddr(g_engine.m_device.m_device, "vkCmdBuildAccelerationStructuresKHR");

    vkGetAccelerationStructureDeviceAddressKHR =
        (PFN_vkGetAccelerationStructureDeviceAddressKHR)vkGetDeviceProcAddr(g_engine.m_device.m_device, "vkGetAccelerationStructureDeviceAddressKHR");

    vkGetAccelerationStructureBuildSizesKHR =
        (PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetDeviceProcAddr(g_engine.m_device.m_device, "vkGetAccelerationStructureBuildSizesKHR");

    vkCreateRayTracingPipelinesKHR =
        (PFN_vkCreateRayTracingPipelinesKHR)vkGetDeviceProcAddr(g_engine.m_device.m_device, "vkCreateRayTracingPipelinesKHR");

    vkGetRayTracingShaderGroupHandlesKHR =
        (PFN_vkGetRayTracingShaderGroupHandlesKHR)vkGetDeviceProcAddr(g_engine.m_device.m_device, "vkGetRayTracingShaderGroupHandlesKHR");

    vkCmdTraceRaysKHR =
        (PFN_vkCmdTraceRaysKHR)vkGetDeviceProcAddr(g_engine.m_device.m_device, "vkCmdTraceRaysKHR");
}

void trayser::Swapchain::Init(const Device& device)
{
    SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device);
    VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = ChooseSwapExtent(device, swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

    if (swapChainSupport.capabilities.maxImageCount > 0 &&
        imageCount > swapChainSupport.capabilities.maxImageCount)
    {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = device.m_surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = FindQueueFamilies(device.m_physDevice, device.m_surface);
    uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    if (indices.graphicsFamily != indices.presentFamily)
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0; // Optional
        createInfo.pQueueFamilyIndices = nullptr; // Optional
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VK_CHECK(vkCreateSwapchainKHR(device.m_device, &createInfo, nullptr, &m_swapchain));

    vkGetSwapchainImagesKHR(device.m_device, m_swapchain, &imageCount, nullptr);
    m_images.resize(imageCount);
    vkGetSwapchainImagesKHR(device.m_device, m_swapchain, &imageCount, m_images.data());

    m_format = surfaceFormat.format;
    m_extent = extent;

    // Build a image-view for the draw image to use for rendering
    m_imageViews.resize(imageCount);
    for (int i = 0; i < m_images.size(); i++)
    {
        VkImageViewCreateInfo createInfo = ImageViewCreateInfo();
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = m_format;
        createInfo.image = m_images[i];
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        VK_CHECK(vkCreateImageView(device.m_device, &createInfo, nullptr, &m_imageViews[i]));
    }

	// Initialize command pools/buffers for each frame
    u32 graphicsQueueFamily = 0; // TODO temp
    VkCommandPoolCreateInfo commandPoolInfo = CommandPoolCreateInfo();
    commandPoolInfo.queueFamilyIndex = graphicsQueueFamily;
    commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    for (int i = 0; i < kFrameCount; i++)
    {
        VK_CHECK(vkCreateCommandPool(device.m_device, &commandPoolInfo, nullptr, &m_frames[i].commandPool));

        VkCommandBufferAllocateInfo cmdAllocInfo = CommandBufferAllocateInfo();
        cmdAllocInfo.commandPool = m_frames[i].commandPool;
        cmdAllocInfo.commandBufferCount = 1;
        cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

        VK_CHECK(vkAllocateCommandBuffers(device.m_device, &cmdAllocInfo, &m_frames[i].commandBuffer));
    }

	// Create synchronization structures
    VkFenceCreateInfo fenceCreateInfo = FenceCreateInfo();
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkSemaphoreCreateInfo semaphoreCreateInfo = SemaphoreCreateInfo();
    semaphoreCreateInfo.flags = 0;

    for (int i = 0; i < kFrameCount; i++)
    {
        VK_CHECK(vkCreateFence(device.m_device, &fenceCreateInfo, nullptr, &m_frames[i].renderFence));
        VK_CHECK(vkCreateSemaphore(device.m_device, &semaphoreCreateInfo, nullptr, &m_frames[i].swapchainSemaphore));
        VK_CHECK(vkCreateSemaphore(device.m_device, &semaphoreCreateInfo, nullptr, &m_frames[i].renderSemaphore));
    }

    PRINT_INFO("Initialized Vulkan swapchain.");
}

void trayser::Swapchain::Destroy(const Device& device)
{
    for (auto& frame : m_frames)
    {
        vkDestroySemaphore(device.m_device, frame.renderSemaphore, nullptr);
        vkDestroySemaphore(device.m_device, frame.swapchainSemaphore, nullptr);
        vkDestroyFence(device.m_device, frame.renderFence, nullptr);
        vkDestroyCommandPool(device.m_device, frame.commandPool, nullptr);
    }

    for (auto imageView : m_imageViews)
        vkDestroyImageView(device.m_device, imageView, nullptr);

    vkDestroySwapchainKHR(device.m_device, m_swapchain, nullptr);
}

trayser::SwapChainSupportDetails trayser::Swapchain::QuerySwapChainSupport(const Device& device) const
{
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.m_physDevice, device.m_surface, &details.capabilities);
    printf("Surface capabilities: minImageCount=%u, maxImageCount=%u\n",
        details.capabilities.minImageCount, details.capabilities.maxImageCount);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device.m_physDevice, device.m_surface, &formatCount, nullptr);

    if (formatCount != 0)
    {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device.m_physDevice, device.m_surface, &formatCount, details.formats.data());
    }
    printf("Surface formats count: %u\n", formatCount);

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device.m_physDevice, device.m_surface, &presentModeCount, nullptr);

    if (presentModeCount != 0)
    {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device.m_physDevice, device.m_surface, &presentModeCount, details.presentModes.data());
    }
    printf("Present modes count: %u\n", presentModeCount);

    return details;
}

VkExtent2D trayser::Swapchain::ChooseSwapExtent(const Device& device, const VkSurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return capabilities.currentExtent;
    }
    else
    {
        int width, height;
        SDL_GetWindowSize(device.m_window, &width, &height);

        VkExtent2D actualExtent =
        {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}
