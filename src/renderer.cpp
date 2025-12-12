#include <pch.h>

#include <images.h>

#include <engine.h>
#include <renderer.h>
#include <../shaders/gpu_io.h>

#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <imgui_impl_sdl2.h>

void trayser::Renderer::Init(Device& device)
{
    InitCmdPool(device);
	InitTextureDescLayout(device);
    InitSwapchain(device);
    InitDefaultImage(device);
	InitFrames(device);
    InitImGui(device);
}

void trayser::Renderer::Destroy(Device& device)
{
    DestroyImGui();
    DestroyFrames(device);
    DestroyDefaultImage(device);
    DestroySwapchain(device);
    DestroyCmdPool(device);
}

void trayser::Renderer::Render(uint32_t frameIndex)
{
    m_frameCounter++;
}

void trayser::Renderer::InitCmdPool(Device& device)
{
    VkCommandPoolCreateInfo createInfo = CommandPoolCreateInfo();
    createInfo.queueFamilyIndex = device.m_graphicsQueueFamily;
    createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VK_CHECK(vkCreateCommandPool(device.m_device, &createInfo, nullptr, &m_cmdPool));
}

void trayser::Renderer::InitSwapchain(Device& device)
{
    SwapchainSupport swapChainSupport   = device.GetSwapchainSupport();
    VkSurfaceFormatKHR surfaceFormat    = ChooseSwapchainFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode        = ChoosePresentMode(swapChainSupport.presentModes);
    VkExtent2D extent                   = ChooseSwapchainExtent(device, swapChainSupport.capabilities);

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

    QueueFamilyIndices indices = device.GetQueueFamilies();
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

    VK_CHECK(vkCreateSwapchainKHR(device.m_device, &createInfo, nullptr, &m_swapchain.swapchain));

    VK_CHECK(vkGetSwapchainImagesKHR(device.m_device, m_swapchain.swapchain, &m_swapchain.frameCount, nullptr));

    m_swapchain.format = surfaceFormat.format;
    m_swapchain.extent = extent;

    m_swapchain.images.resize(m_swapchain.frameCount);
    vkGetSwapchainImagesKHR(device.m_device, m_swapchain.swapchain, &m_swapchain.frameCount, m_swapchain.images.data());

    m_swapchain.views.resize(m_swapchain.frameCount);
    for (int i = 0; i < m_swapchain.frameCount; i++)
    {
        VkImageViewCreateInfo createInfo = ImageViewCreateInfo();
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = m_swapchain.format;
        createInfo.image = m_swapchain.images[i];
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        VK_CHECK(vkCreateImageView(device.m_device, &createInfo, nullptr, &m_swapchain.views[i]));
    }

    PRINT_INFO("Initialized Vulkan swapchain.");
}

void trayser::Renderer::InitTextureDescLayout(Device& device)
{
    DescriptorSetLayoutBinding binding{};
    binding.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = kTextureCount;
    binding.stageFlags =
        VK_SHADER_STAGE_FRAGMENT_BIT |
        VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
        VK_SHADER_STAGE_MISS_BIT_KHR |
        VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    binding.pImmutableSamplers = nullptr;

    DescriptorSetLayoutBuilder builder{};
    builder.AddBinding(binding);
    builder.Build(device, m_textureDescLayout);
}

void trayser::Renderer::InitDefaultImage(Device& device)
{
    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
    VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    VkImageCreateInfo imageCreateInfo = ImageCreateInfo();
    imageCreateInfo.format      = format;
    imageCreateInfo.usage       = usage;
    imageCreateInfo.extent      = VkExtent3D{1, 1, 1};
    imageCreateInfo.imageType   = VK_IMAGE_TYPE_2D;
    imageCreateInfo.mipLevels   = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples     = VK_SAMPLE_COUNT_1_BIT;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK(vmaCreateImage(device.m_allocator, &imageCreateInfo, &allocInfo, &m_defaultImage.image, &m_defaultImage.allocation, nullptr));

    VkImageViewCreateInfo viewCreateInfo = ImageViewCreateInfo();
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format = format;
    viewCreateInfo.image = m_defaultImage.image;
    viewCreateInfo.subresourceRange.baseMipLevel = 0;
    viewCreateInfo.subresourceRange.levelCount = 1;
    viewCreateInfo.subresourceRange.baseArrayLayer = 0;
    viewCreateInfo.subresourceRange.layerCount = 1;
    viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewCreateInfo.subresourceRange.levelCount = imageCreateInfo.mipLevels;
    VK_CHECK(vkCreateImageView(device.m_device, &viewCreateInfo, nullptr, &m_defaultImage.view));

    VkSamplerCreateInfo samplerCreateInfo = SamplerCreateInfo();
    samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
    samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
    vkCreateSampler(device.m_device, &samplerCreateInfo, nullptr, &m_defaultImage.sampler);
}

void trayser::Renderer::InitFrames(Device& device)
{
    InitCmdBuffers(device);
    InitSyncStructures(device);
    InitTextureDescSets(device);
}

void trayser::Renderer::InitCmdBuffers(Device& device)
{
    VkCommandBufferAllocateInfo allocInfo = CommandBufferAllocateInfo();
    allocInfo.commandPool        = m_cmdPool;
    allocInfo.commandBufferCount = kMaxFramesInFlight;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
;
    VkCommandBuffer* cmdBuffers = new VkCommandBuffer[kMaxFramesInFlight];
    VK_CHECK(vkAllocateCommandBuffers(device.m_device, &allocInfo, cmdBuffers));

    for (int i = 0; i < kMaxFramesInFlight; i++)
    {
        m_frames[i].cmdBuffer = cmdBuffers[i];
    }

    delete[] cmdBuffers;
}

void trayser::Renderer::InitSyncStructures(Device& device)
{
    VkFenceCreateInfo fenceCreateInfo = FenceCreateInfo();
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkSemaphoreCreateInfo semaphoreCreateInfo = SemaphoreCreateInfo();
    semaphoreCreateInfo.flags = 0;

    for (int i = 0; i < kMaxFramesInFlight; i++)
    {
        VK_CHECK(vkCreateFence(device.m_device, &fenceCreateInfo, nullptr, &m_frames[i].renderedFence));
        VK_CHECK(vkCreateSemaphore(device.m_device, &semaphoreCreateInfo, nullptr, &m_frames[i].acquisitionSemaphore));
        VK_CHECK(vkCreateSemaphore(device.m_device, &semaphoreCreateInfo, nullptr, &m_frames[i].renderedSemaphore));
    }
}

void trayser::Renderer::InitTextureDescSets(Device& device)
{
    for (int i = 0; i < kMaxFramesInFlight; i++)
    {
        m_frames[i].textureDescSet = g_engine.m_globalDescriptorAllocator.Allocate(device.m_device, m_textureDescLayout);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = m_defaultImage.view;
        imageInfo.sampler = m_defaultImage.sampler;

        std::vector<VkDescriptorImageInfo> imageInfos(kTextureCount);
        std::fill(imageInfos.begin(), imageInfos.end(), imageInfo);

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_frames[i].textureDescSet;
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = kTextureCount;
        write.pImageInfo = imageInfos.data();

        vkUpdateDescriptorSets(device.m_device, 1, &write, 0, nullptr);
    }
}

void trayser::Renderer::InitImGui(Device& device)
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
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
    .maxSets = 1000,
    .poolSizeCount = (u32)std::size(poolSizes),
    .pPoolSizes = poolSizes
    };

    VkDescriptorPool pool;
    VK_CHECK(vkCreateDescriptorPool(device.m_device, &poolInfo, nullptr, &pool));

    ImGui_ImplVulkan_InitInfo initInfo =
    {
        .Instance               = device.m_instance,
        .PhysicalDevice         = device.m_physDevice,
        .Device                 = device.m_device,
        .Queue                  = device.m_graphicsQueue,
        .DescriptorPool         = pool,
        .MinImageCount          = m_swapchain.frameCount,
        .ImageCount             = m_swapchain.frameCount,
        .UseDynamicRendering    = true
    };

    initInfo.PipelineRenderingCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    initInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    initInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = &m_swapchain.format;
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForVulkan(device.m_window);
    ImGui_ImplVulkan_Init(&initInfo);
    ImGui_ImplVulkan_CreateFontsTexture();

    PRINT_INFO("Initialized ImGui.");
}

void trayser::Renderer::DestroyCmdPool(Device& device) const
{
    vkDestroyCommandPool(device.m_device, m_cmdPool, nullptr);
}

void trayser::Renderer::DestroySwapchain(Device& device) const
{
    for (int i = 0; i < m_swapchain.frameCount; i++)
    {
        vkDestroyImageView(device.m_device, m_swapchain.views[i], nullptr);
    }

    vkDestroySwapchainKHR(device.m_device, m_swapchain.swapchain, nullptr);
}

void trayser::Renderer::DestroyFrames(Device& device) const
{
    DestroyTextureDescSets(device);
    DestroySyncStructures(device);

    delete[] m_frames;
}

void trayser::Renderer::DestroySyncStructures(Device& device) const
{
    for (int i = 0; i < m_swapchain.frameCount; i++)
    {
        vkDestroySemaphore(device.m_device, m_frames[i].renderedSemaphore, nullptr);
        vkDestroySemaphore(device.m_device, m_frames[i].acquisitionSemaphore, nullptr);
        vkDestroyFence(device.m_device, m_frames[i].renderedFence, nullptr);
    }
}

void trayser::Renderer::DestroyDefaultImage(Device& device) const
{
    vmaDestroyImage(device.m_allocator, m_defaultImage.image, m_defaultImage.allocation);
    vkDestroyImageView(device.m_device, m_defaultImage.view, nullptr);
    vkDestroySampler(device.m_device, m_defaultImage.sampler, nullptr);
}

void trayser::Renderer::DestroyTextureDescSets(Device& device) const
{
    // TODO
}

void trayser::Renderer::DestroyImGui() const
{
    ImGui_ImplVulkan_Shutdown();
    ImGui::DestroyContext();
}

VkSurfaceFormatKHR trayser::Renderer::ChooseSwapchainFormat(const std::vector<VkSurfaceFormatKHR>& formats)
{
    for (const auto& availableFormat : formats)
    {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return availableFormat;
        }
    }

    return formats[0];
}

VkPresentModeKHR trayser::Renderer::ChoosePresentMode(const std::vector<VkPresentModeKHR>& presentModes)
{
    for (const auto& availablePresentMode : presentModes)
    {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return availablePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D trayser::Renderer::ChooseSwapchainExtent(const Device& device, const VkSurfaceCapabilitiesKHR& capabilities)
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

void trayser::Renderer::ImGuiNewFrame()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void trayser::Renderer::NewFrame(Device& device)
{
    VK_CHECK(vkWaitForFences(device.m_device, 1, &m_frames[m_frameIndex].renderedFence, true, UINT64_MAX));
    VK_CHECK(vkResetFences(device.m_device, 1, &m_frames[m_frameIndex].renderedFence));
    VK_CHECK(vkAcquireNextImageKHR(device.m_device, m_swapchain.swapchain, UINT64_MAX, GetAcquisitionSemaphore(), nullptr, &m_swapchain.imageIndex));
    VK_CHECK(vkResetCommandBuffer(GetCmdBuffer(), 0));

    ImGuiNewFrame();

    VkCommandBufferBeginInfo cmdBeginInfo = CommandBufferBeginInfo();
    cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(GetCmdBuffer(), &cmdBeginInfo));
}

void trayser::Renderer::EndFrame(Device& device)
{
    vkutil::TransitionImage(GetCmdBuffer(), GetSwapchainImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    RenderImGui();
    vkutil::TransitionImage(GetCmdBuffer(), GetSwapchainImage(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VK_CHECK(vkEndCommandBuffer(GetCmdBuffer()));

    VkCommandBufferSubmitInfo cmdSubmitInfo = CommandBufferSubmitInfo();
    cmdSubmitInfo.commandBuffer = GetCmdBuffer();
    cmdSubmitInfo.deviceMask = 0;

    VkSemaphoreSubmitInfo waitInfo = SemaphoreSubmitInfo();
    waitInfo.semaphore = GetAcquisitionSemaphore();
    waitInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
    waitInfo.deviceIndex = 0;
    waitInfo.value = 1;

    VkSemaphoreSubmitInfo signalInfo = SemaphoreSubmitInfo();
    signalInfo.semaphore = GetRenderedSemaphore();
    signalInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
    signalInfo.deviceIndex = 0;
    signalInfo.value = 1;

    VkSubmitInfo2 submitInfo = SubmitInfo2();
    submitInfo.pWaitSemaphoreInfos = &waitInfo;
    submitInfo.waitSemaphoreInfoCount = 1;
    submitInfo.pSignalSemaphoreInfos = &signalInfo;
    submitInfo.signalSemaphoreInfoCount = 1;
    submitInfo.pCommandBufferInfos = &cmdSubmitInfo;
    submitInfo.commandBufferInfoCount = 1;
    VK_CHECK(vkQueueSubmit2(device.m_graphicsQueue, 1, &submitInfo, GetRenderedFence()));

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType               = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext               = nullptr;
    presentInfo.pSwapchains         = &m_swapchain.swapchain;
    presentInfo.swapchainCount      = 1;
    presentInfo.pWaitSemaphores     = &m_frames[m_frameIndex].renderedSemaphore;
    presentInfo.waitSemaphoreCount  = 1;
    presentInfo.pImageIndices       = &m_swapchain.imageIndex;
    vkQueuePresentKHR(device.m_graphicsQueue, &presentInfo);

    AdvanceFrame();
}

void trayser::Renderer::RenderImGui()
{
    ImGui::Render();
    VkRenderingAttachmentInfo colorAttachment = RenderingAttachmentInfo();
    colorAttachment.imageView   = GetSwapchainImageView();
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp      = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo renderInfo      = RenderingInfo();
    renderInfo.renderArea           = VkRect2D{ VkOffset2D { 0, 0 }, m_swapchain.extent };
    renderInfo.pColorAttachments    = &colorAttachment;
    renderInfo.pStencilAttachment   = nullptr;
    renderInfo.pDepthAttachment     = nullptr;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.layerCount           = 1;

    vkCmdBeginRendering(GetCmdBuffer(), &renderInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), GetCmdBuffer());
    vkCmdEndRendering(GetCmdBuffer());
}

void trayser::Renderer::AdvanceFrame()
{
    m_frameCounter++;
    m_frameIndex = (m_frameIndex + 1) % kMaxFramesInFlight;
}
