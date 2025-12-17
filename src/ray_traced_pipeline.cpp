#include <pch.h>

#include <ray_traced_pipeline.h>
#include <engine.h>

trayser::RayTracedPipeline::RayTracedPipeline() :
    m_descriptorSetLayout(VK_NULL_HANDLE),
    m_descriptorSet(VK_NULL_HANDLE)
{
    m_name = "path_traced";
    m_canHotReload = true;
}

void trayser::RayTracedPipeline::Load(VkShaderModule module)
{
    // Descriptor set layout

    DescriptorLayoutBuilder builder;
    builder.AddBinding(BindingPoints_TLAS, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
    builder.AddBinding(BindingPoints_OutImage, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    builder.AddBinding(BindingPoints_OutAccumulator, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    m_descriptorSetLayout = builder.Build(g_engine.m_device.m_device, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);

    m_descriptorSet = g_engine.m_globalDescriptorAllocator.Allocate(g_engine.m_device.m_device, m_descriptorSetLayout);

    DescriptorWriter writer;
    writer.WriteImage(BindingPoints_OutImage, g_engine.m_gBuffer.colorView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    writer.WriteImage(BindingPoints_OutAccumulator, g_engine.m_gBuffer.accumulatorView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    writer.UpdateSet(g_engine.m_device.m_device, m_descriptorSet);

    // ---------------------

    // For re-creation
    //vkDestroyPipeline(m_app->getDevice(), m_rtPipeline, nullptr);
    //vkDestroyPipelineLayout(m_app->getDevice(), m_rtPipelineLayout, nullptr);

    // Creating all shaders
    enum StageIndices
    {
        eRaygen,
        eMiss,
        eClosestHit,
        eShaderGroupCount
    };
    std::array<VkPipelineShaderStageCreateInfo, eShaderGroupCount> stages{};
    for (auto& s : stages)
        s.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;

    stages[eRaygen].pNext = nullptr;
    stages[eRaygen].pName = "rgMain";
    stages[eRaygen].stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    stages[eRaygen].module = module;
    stages[eMiss].pNext = nullptr;
    stages[eMiss].pName = "mMain";
    stages[eMiss].stage = VK_SHADER_STAGE_MISS_BIT_KHR;
    stages[eMiss].module = module;
    stages[eClosestHit].pNext = nullptr;
    stages[eClosestHit].pName = "chMain";
    stages[eClosestHit].stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    stages[eClosestHit].module = module;

    // Shader groups
    VkRayTracingShaderGroupCreateInfoKHR group{ VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR };
    group.anyHitShader = VK_SHADER_UNUSED_KHR;
    group.closestHitShader = VK_SHADER_UNUSED_KHR;
    group.generalShader = VK_SHADER_UNUSED_KHR;
    group.intersectionShader = VK_SHADER_UNUSED_KHR;

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> shader_groups;
    // Raygen
    group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    group.generalShader = eRaygen;
    shader_groups.push_back(group);

    // Miss
    group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    group.generalShader = eMiss;
    shader_groups.push_back(group);

    // closest hit shader
    group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    group.generalShader = VK_SHADER_UNUSED_KHR;
    group.closestHitShader = eClosestHit;
    shader_groups.push_back(group);

    VkPushConstantRange pushConst{};
    pushConst.offset = 0;
    pushConst.size = sizeof(gpu::RTPushConstants);
    pushConst.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    VkPipelineLayoutCreateInfo pipeline_layout_create_info{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipeline_layout_create_info.pushConstantRangeCount = 1;
    pipeline_layout_create_info.pPushConstantRanges = &pushConst;

    // Descriptor sets: one specific to ray tracing, and one shared with the rasterization pipeline
    VkDescriptorSetLayout descLayouts[2] = {g_engine.m_renderer.m_textureDescLayout, m_descriptorSetLayout};
    pipeline_layout_create_info.setLayoutCount = 2;
    pipeline_layout_create_info.pSetLayouts = descLayouts;
    VK_CHECK(vkCreatePipelineLayout(g_engine.m_device.m_device, &pipeline_layout_create_info, nullptr, &m_layout));
    //NVVK_DBG_NAME(m_rtPipelineLayout);

    // Assemble the shader stages and recursion depth info into the ray tracing pipeline
    VkRayTracingPipelineCreateInfoKHR rtPipelineInfo{ VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR };
    rtPipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
    rtPipelineInfo.pStages = stages.data();
    rtPipelineInfo.groupCount = static_cast<uint32_t>(shader_groups.size());
    rtPipelineInfo.pGroups = shader_groups.data();
    rtPipelineInfo.maxPipelineRayRecursionDepth = std::max(3U, g_engine.m_device.m_rtProperties.maxRayRecursionDepth);
    rtPipelineInfo.layout = m_layout;
    VK_CHECK(vkfuncs::vkCreateRayTracingPipelinesKHR(g_engine.m_device.m_device, {}, {}, 1, &rtPipelineInfo, nullptr, &m_pipeline));
    //NVVK_DBG_NAME(m_rtPipeline);

    //LOGI("Ray tracing pipeline created successfully\n");

    // Shader binding table

    //m_allocator.destroyBuffer(m_sbtBuffer);  // Cleanup when re-creating

    VkDevice device = g_engine.m_device.m_device;
    uint32_t handleSize = g_engine.m_device.m_rtProperties.shaderGroupHandleSize;
    uint32_t handleAlignment = g_engine.m_device.m_rtProperties.shaderGroupHandleAlignment;
    uint32_t baseAlignment = g_engine.m_device.m_rtProperties.shaderGroupBaseAlignment;
    uint32_t groupCount = rtPipelineInfo.groupCount;

    // Get shader group handles
    size_t dataSize = handleSize * groupCount;
    m_shaderHandles.resize(dataSize);
    VK_CHECK(vkfuncs::vkGetRayTracingShaderGroupHandlesKHR(device, m_pipeline, 0, groupCount, dataSize, m_shaderHandles.data()));

    // Calculate SBT buffer size with proper alignment
    auto     alignUp = [](uint32_t size, uint32_t alignment) { return (size + alignment - 1) & ~(alignment - 1); };
    uint32_t raygenSize = alignUp(handleSize, handleAlignment);
    uint32_t missSize = alignUp(handleSize, handleAlignment);
    uint32_t hitSize = alignUp(handleSize, handleAlignment);
    uint32_t callableSize = 0;  // No callable shaders in this tutorial

    // Ensure each region starts at a baseAlignment boundary
    uint32_t raygenOffset = 0;
    uint32_t missOffset = alignUp(raygenSize, baseAlignment);
    uint32_t hitOffset = alignUp(missOffset + missSize, baseAlignment);
    uint32_t callableOffset = alignUp(hitOffset + hitSize, baseAlignment);

    size_t bufferSize = callableOffset + callableSize;
    VmaAllocationInfo allocInfo{};

    // Create SBT buffer
    //VK_CHECK(g_engine.m_device.CreateBuffer(m_sbtBuffer, bufferSize, VK_BUFFER_USAGE_2_SHADER_BINDING_TABLE_BIT_KHR, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
    //    VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT));
    VK_CHECK(g_engine.m_device.CreateBuffer(
        bufferSize, 
        VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR, 
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
        m_sbtBuffer,
        &allocInfo));
    //NVVK_DBG_NAME(m_sbtBuffer.buffer);

    const VkBufferDeviceAddressInfo info = { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = m_sbtBuffer.buffer };
    VkDeviceAddress sbtBufferAddr = vkGetBufferDeviceAddress(g_engine.m_device.m_device, &info);

    // Populate SBT buffer
    uint8_t* pData = static_cast<uint8_t*>(allocInfo.pMappedData);

    // Ray generation shader (group 0)
    memcpy(pData + raygenOffset, m_shaderHandles.data() + 0 * handleSize, handleSize);
    m_raygenRegion.deviceAddress = sbtBufferAddr + raygenOffset;
    m_raygenRegion.stride = raygenSize;
    m_raygenRegion.size = raygenSize;

    // Miss shader (group 1)
    memcpy(pData + missOffset, m_shaderHandles.data() + 1 * handleSize, handleSize);
    m_missRegion.deviceAddress = sbtBufferAddr + missOffset;
    m_missRegion.stride = missSize;
    m_missRegion.size = missSize;

    // Hit shader (group 2)
    memcpy(pData + hitOffset, m_shaderHandles.data() + 2 * handleSize, handleSize);
    m_hitRegion.deviceAddress = sbtBufferAddr + hitOffset;
    m_hitRegion.stride = hitSize;
    m_hitRegion.size = hitSize;

    // Callable shaders (none in this tutorial)
    m_callableRegion.deviceAddress = 0;
    m_callableRegion.stride = 0;
    m_callableRegion.size = 0;

    ///////////////////////
}

void trayser::RayTracedPipeline::Update()
{
    //if (g_engine.m_frame >= m_sppThreshold)
    //    return;

    ClearIfAccumulatorReset();

    // Ray trace pipeline
    vkCmdBindPipeline(g_engine.m_renderer.GetCmdBuffer(), VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_pipeline);

    VkAccelerationStructureDeviceAddressInfoKHR ai{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
    ai.accelerationStructure = g_engine.m_renderer.GetTlas().accelStruct;
    VkDeviceAddress addr = vkfuncs::vkGetAccelerationStructureDeviceAddressKHR(g_engine.m_device.m_device, &ai);
    assert(addr != 0); // 0 means invalid/never built

    {
        DescriptorWriter writer;
        writer.WriteAccelStruct(0, g_engine.m_renderer.GetTlas().accelStruct);
        writer.UpdateSet(g_engine.m_device.m_device, m_descriptorSet);
    }

    VkDescriptorSet descSets[2] = { g_engine.m_renderer.GetTextureDescSet(), m_descriptorSet};
    vkCmdBindDescriptorSets(g_engine.m_renderer.GetCmdBuffer(), VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_layout, 0, 2, descSets, 0, nullptr);

    gpu::RTPushConstants pushConsts{};
    pushConsts.sceneRef = g_engine.m_sceneBufferAddr;
    pushConsts.renderMode = g_engine.m_renderMode;
    pushConsts.frame = g_engine.m_frame;

    vkCmdPushConstants(g_engine.m_renderer.GetCmdBuffer(),
        m_layout,
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        0,
        sizeof(gpu::RTPushConstants),
        &pushConsts);

    // Ray trace
    const VkExtent2D size = { kInitWindowWidth, kInitWindowHeight };
    vkfuncs::vkCmdTraceRaysKHR(g_engine.m_renderer.GetCmdBuffer(), &m_raygenRegion, &m_missRegion, &m_hitRegion, &m_callableRegion, size.width, size.height, 1);

    // Barrier to make sure the image is ready for Tonemapping
    PipelineBarrier();
}

VkShaderModule trayser::RayTracedPipeline::Compile()
{
    return g_engine.m_compiler.CompileAll(m_name.c_str());
}

void trayser::RayTracedPipeline::PipelineBarrier() const
{
    // Barrier to make sure the image is ready for Tonemapping
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
    barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT; // or COMPUTE if tonemapping
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL; // whatever layout ray tracing used
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL; // for sampling
    barrier.image = g_engine.m_gBuffer.colorImage.image;
    barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    // Barrier to make sure the image is ready for Tonemapping
    VkImageMemoryBarrier2 barrier2{};
    barrier2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier2.srcStageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
    barrier2.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    barrier2.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT; // or COMPUTE if tonemapping
    barrier2.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    barrier2.oldLayout = VK_IMAGE_LAYOUT_GENERAL; // whatever layout ray tracing used
    barrier2.newLayout = VK_IMAGE_LAYOUT_GENERAL; // for sampling
    barrier2.image = g_engine.m_gBuffer.accumulatorImage.image;
    barrier2.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    VkImageMemoryBarrier2 imageBarriers[2] = { barrier, barrier2 };
    VkDependencyInfo depInfo{};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.imageMemoryBarrierCount = 2;
    depInfo.pImageMemoryBarriers = imageBarriers;

    vkCmdPipelineBarrier2(g_engine.m_renderer.GetCmdBuffer(), &depInfo);
}

void trayser::RayTracedPipeline::ClearIfAccumulatorReset()
{
    if (g_engine.m_frame != 0)
        return;

    // Clear the image to a specified color (e.g., black)
    VkClearColorValue clearColor = {};
    clearColor.float32[0] = 0.0f; // Set to black
    clearColor.float32[1] = 0.0f;
    clearColor.float32[2] = 0.0f;
    clearColor.float32[3] = 1.0f;

    // Create the image memory barrier for transitioning to the correct layout
    VkImageMemoryBarrier imageMemoryBarrier = {};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.srcAccessMask = 0;
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.image = g_engine.m_gBuffer.accumulatorImage.image;
    imageMemoryBarrier.subresourceRange = {
        VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1
    };

    // Transition the image layout to transfer destination
    vkCmdPipelineBarrier(
        g_engine.m_renderer.GetCmdBuffer(),
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, // dependencyFlags
        0, // memoryBarrierCount
        nullptr, // pMemoryBarriers
        0, // bufferMemoryBarrierCount
        nullptr, // pBufferMemoryBarriers
        1, // imageMemoryBarrierCount
        &imageMemoryBarrier// pImageMemoryBarriers
    );

    // Clear the image to the specified color
    vkCmdClearColorImage(
        g_engine.m_renderer.GetCmdBuffer(),
        g_engine.m_gBuffer.accumulatorImage.image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        &clearColor,
        1, &imageMemoryBarrier.subresourceRange
    );

    // Transition the image back to a layout for use in a compute shader
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;  // Or VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL if reading
    imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

    vkCmdPipelineBarrier(
        g_engine.m_renderer.GetCmdBuffer(),
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier
    );
}
