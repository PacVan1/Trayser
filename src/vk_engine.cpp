#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_initializers.h>
#include <vk_types.h>
#include <vk_images.h>

#include "VkBootstrap.h"

#include <chrono>
#include <thread>

static VulkanEngine* g_loadedEngine = nullptr;
static constexpr bool kUseValidationLayers = true;

VulkanEngine& VulkanEngine::Get() { return *g_loadedEngine; }

void VulkanEngine::Init()
{
    assert(g_loadedEngine == nullptr);
    g_loadedEngine = this;

    SDL_Init(SDL_INIT_VIDEO);
    m_window = SDL_CreateWindow(
        "Vulkan Engine",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        m_windowExtent.width,
        m_windowExtent.height,
        SDL_WINDOW_VULKAN);

    InitVulkan();
    InitSwapchain();
    InitCommands();
    InitSyncStructures();

    m_isInitialized = true;
}

void VulkanEngine::Cleanup()
{
    if (m_isInitialized) 
    {    
        // Make sure the gpu is done with its work
        vkDeviceWaitIdle(m_device);

        for (int i = 0; i < kFrameCount; i++) 
        {
            vkDestroyCommandPool(m_device, m_frames[i].commandPool, nullptr);
            vkDestroyFence(m_device, m_frames[i].renderFence, nullptr);
            vkDestroySemaphore(m_device, m_frames[i].renderSemaphore, nullptr);
            vkDestroySemaphore(m_device, m_frames[i].swapchainSemaphore, nullptr);
        }

        DestroySwapchain();

        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        vkDestroyDevice(m_device, nullptr);

        vkb::destroy_debug_utils_messenger(m_instance, m_debugMessenger);
        vkDestroyInstance(m_instance, nullptr);
        SDL_DestroyWindow(m_window);
    }

    // Clear engine pointer
    g_loadedEngine = nullptr;
}

void VulkanEngine::Render()
{
    VK_CHECK(vkWaitForFences(m_device, 1, &GetCurrentFrame().renderFence, true, 1000000000));
    VK_CHECK(vkResetFences(m_device, 1, &GetCurrentFrame().renderFence));

    uint32_t swapchainImageIndex;
    VK_CHECK(vkAcquireNextImageKHR(m_device, m_swapchain, 1000000000, GetCurrentFrame().swapchainSemaphore, nullptr, &swapchainImageIndex));
    
    VkCommandBuffer cmd = GetCurrentFrame().commandBuffer;
    BeginRecording(cmd);

    // Make the swapchain image into writeable mode before rendering
    vkutil::TransitionImage(cmd, m_swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    VkClearColorValue clearValue;
    clearValue = { { 0.0f, 0.0f, 1.0f, 1.0f } };

    VkImageSubresourceRange clearRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);

    // Clear image
    vkCmdClearColorImage(cmd, m_swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);

    // Make the swapchain image into presentable mode
    vkutil::TransitionImage(cmd, m_swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    // Finalize the command buffer (we can no longer add commands, but it can now be executed)
    VK_CHECK(vkEndCommandBuffer(cmd));

    // Prepare the submission to the queue. 
    // We want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
    // We will signal the _renderSemaphore, to signal that rendering has finished
    VkCommandBufferSubmitInfo cmdInfo = vkinit::command_buffer_submit_info(cmd);

    VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, GetCurrentFrame().swapchainSemaphore);
    VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, GetCurrentFrame().renderSemaphore);

    VkSubmitInfo2 submit = vkinit::submit_info(&cmdInfo, &signalInfo, &waitInfo);

    // Submit command buffer to the queue and execute it.
    // _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(m_graphicsQueue, 1, &submit, GetCurrentFrame().renderFence));

    // Prepare present
    // This will put the image we just rendered to into the visible window.
    // We want to wait on the _renderSemaphore for that, 
    // as its necessary that drawing commands have finished before the image is displayed to the user
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.pSwapchains = &m_swapchain;
    presentInfo.swapchainCount = 1;

    presentInfo.pWaitSemaphores = &GetCurrentFrame().renderSemaphore;
    presentInfo.waitSemaphoreCount = 1;

    presentInfo.pImageIndices = &swapchainImageIndex;

    VK_CHECK(vkQueuePresentKHR(m_graphicsQueue, &presentInfo));

    m_frameIdx = (m_frameIdx + 1) % kFrameCount;
}

void VulkanEngine::Run()
{
    SDL_Event e;
    bool quit = false;

    // Main loop
    while (!quit)
    {
        // Handle events on queue
        while (SDL_PollEvent(&e) != 0) 
        {
            // Close the window when user alt-f4s or clicks the X button
            if (e.type == SDL_QUIT)
                quit = true;

            if (e.type == SDL_WINDOWEVENT) 
            {
                if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) 
                {
                    m_stopRendering = true;
                }
                if (e.window.event == SDL_WINDOWEVENT_RESTORED) 
                {
                    m_stopRendering = false;
                }
            }
        }

        // Do not render if we are minimized
        if (m_stopRendering) 
        {
            // Throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        Render();
    }
}

void VulkanEngine::InitVulkan()
{
    vkb::InstanceBuilder builder;

    // Make the vulkan instance, with basic debug features
    auto builderResult = builder
        .set_app_name("Example Vulkan Application")
        .request_validation_layers(kUseValidationLayers)
        .use_default_debug_messenger()
        .require_api_version(1, 3, 0)
        .build();

    // Grab the instance 
    vkb::Instance vkbInst = builderResult.value();
    m_instance       = vkbInst.instance;
    m_debugMessenger = vkbInst.debug_messenger;

    SDL_Vulkan_CreateSurface(m_window, m_instance, &m_surface);

    VkPhysicalDeviceVulkan12Features features12{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    features12.bufferDeviceAddress  = true;
    features12.descriptorIndexing   = true;

    VkPhysicalDeviceVulkan13Features features13{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    features13.dynamicRendering = true;
    features13.synchronization2 = true;

    // Use vkbootstrap to select a gpu. 
    // We want a gpu that can write to the SDL surface and supports vulkan 1.3 with the correct features
    vkb::PhysicalDeviceSelector selector{vkbInst};
    vkb::PhysicalDevice physicalDevice = selector
        .set_minimum_version(1, 3)
        .set_required_features_13(features13)
        .set_required_features_12(features12)
        .set_surface(m_surface)
        .select()
        .value();

    // Create the final vulkan device
    vkb::DeviceBuilder deviceBuilder{physicalDevice};
    vkb::Device vkbDevice = deviceBuilder.build().value();

    // Get the VkDevice handle used in the rest of a vulkan application
    m_device    = vkbDevice.device;
    m_chosenGPU = physicalDevice.physical_device;

    // Use vkbootstrap to get a Graphics queue
    m_graphicsQueue         = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    m_graphicsQueueFamily   = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
}

void VulkanEngine::InitSwapchain()
{
    CreateSwapchain(m_windowExtent.width, m_windowExtent.height);
}

void VulkanEngine::InitCommands()
{
    // Create a command pool for commands submitted to the graphics queue.
    // We also want the pool to allow for resetting of individual command buffers
    VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(m_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (int i = 0; i < kFrameCount; i++) 
    {
        VK_CHECK(vkCreateCommandPool(m_device, &commandPoolInfo, nullptr, &m_frames[i].commandPool));

        // Allocate the default command buffer that we will use for rendering
        VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(m_frames[i].commandPool, 1);

        VK_CHECK(vkAllocateCommandBuffers(m_device, &cmdAllocInfo, &m_frames[i].commandBuffer));
    }
}

void VulkanEngine::InitSyncStructures()
{
    // Create syncronization structures
    // One fence to control when the gpu has finished rendering the frame,
    // and 2 semaphores to syncronize rendering with swapchain
    // We want the fence to start signalled so we can wait on it on the first frame
    VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

    for (int i = 0; i < kFrameCount; i++) 
    {
        VK_CHECK(vkCreateFence(m_device, &fenceCreateInfo, nullptr, &m_frames[i].renderFence));
        VK_CHECK(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_frames[i].swapchainSemaphore));
        VK_CHECK(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_frames[i].renderSemaphore));
    }
}

void VulkanEngine::CreateSwapchain(u32 width, u32 height)
{
    m_swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    vkb::SwapchainBuilder builder{m_chosenGPU, m_device, m_surface};
    vkb::Swapchain vkbSwapchain = builder
        //.use_default_format_selection()
        .set_desired_format(VkSurfaceFormatKHR{ .format = m_swapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
        // Use vsync present mode
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_extent(width, height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build()
        .value();

    m_swapchainExtent       = vkbSwapchain.extent;
    m_swapchain             = vkbSwapchain.swapchain;
    m_swapchainImages       = vkbSwapchain.get_images().value();
    m_swapchainImageViews   = vkbSwapchain.get_image_views().value();
}

void VulkanEngine::DestroySwapchain()
{
    vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);

    for (int i = 0; i < m_swapchainImageViews.size(); i++) 
    {
        vkDestroyImageView(m_device, m_swapchainImageViews[i], nullptr);
    }
}

void VulkanEngine::BeginRecording(VkCommandBuffer cmd)
{
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));
}
