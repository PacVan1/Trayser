#include <pch.h>
#include "engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <initializers.h>
#include <types.h>
#include <images.h>
#include <pipelines.h>

#include "VkBootstrap.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"

#include "vk_mem_alloc.h"

#include <stb_image.h>

#include <chrono>
#include <thread>
#include <iostream>

Engine g_engine;

void Engine::Init()
{
    m_compiler.Init();

    m_device.Init();

    InitCommands();
    InitSyncStructures();   

    // Initialize VMA
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = m_device.m_physDevice;
    allocatorInfo.device = m_device.m_device;
    allocatorInfo.instance = m_device.m_instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &m_allocator);

    m_deletionQueue.Push([&]() { vmaDestroyAllocator(m_allocator); });
    VK_CHECK(vkWaitForFences(m_device.m_device, 1, &m_immFence, true, 9999999999));

    CreateSwapchainImageView();
    InitDescriptors();
    InitPipelines();
    InitImGui();
    InitDefaultData();

    Model::MikkTSpaceInit();

    m_isInitialized = true;
}

void Engine::Cleanup()
{
    if (m_isInitialized) 
    {    
        // Make sure the gpu is done with its work
        vkDeviceWaitIdle(m_device.m_device);

        for (int i = 0; i < kFrameCount; i++) 
        {
            vkDestroyCommandPool(m_device.m_device, m_frames[i].commandPool, nullptr);
            vkDestroyFence(m_device.m_device, m_frames[i].renderFence, nullptr);
            vkDestroySemaphore(m_device.m_device, m_frames[i].renderSemaphore, nullptr);
            vkDestroySemaphore(m_device.m_device, m_frames[i].swapchainSemaphore, nullptr);
            m_frames[i].deletionQueue.Flush();
        }

        m_deletionQueue.Flush();

        DestroySwapchain();

        vkDestroySurfaceKHR(m_device.m_instance, m_device.m_surface, nullptr);
        vkDestroyDevice(m_device.m_device, nullptr);

        //vkb::destroy_debug_utils_messenger(m_device.m_instance, m_device.m_debugMessenger);
        vkDestroyInstance(m_device.m_instance, nullptr);
        SDL_DestroyWindow(m_device.m_window);
    }
}

void Engine::Render()
{
    VK_CHECK(vkWaitForFences(m_device.m_device, 1, &GetCurrentFrame().renderFence, true, 1000000000));
    GetCurrentFrame().deletionQueue.Flush();
    GetCurrentFrame().descriptors.ClearPools(m_device.m_device);

    VK_CHECK(vkResetFences(m_device.m_device, 1, &GetCurrentFrame().renderFence));

    uint32_t swapchainImageIndex;
    VkResult e = vkAcquireNextImageKHR(m_device.m_device, m_device.m_swapchain, 1000000000, GetCurrentFrame().swapchainSemaphore, nullptr, &swapchainImageIndex);
    if (e == VK_ERROR_OUT_OF_DATE_KHR) 
    {
        m_resizeRequested = true;
        return;
    }
    
    m_renderExtent.width  = m_renderImage.imageExtent.width;
    m_renderExtent.height = m_renderImage.imageExtent.height;

    VkCommandBuffer cmd = GetCurrentFrame().commandBuffer;
    BeginRecording(cmd);

    // Make the swapchain image into writeable mode before rendering
    vkutil::TransitionImage(cmd, m_device.m_swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    
    m_pipelines[PipelineType_Background]->Update();

    vkutil::TransitionImage(cmd, m_renderImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vkutil::TransitionImage(cmd, m_depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    //RenderTriangle(cmd);
    m_pipelines[PipelineType_PBR]->Update();

    // Transition the draw image and the swapchain image into their correct transfer layouts
    vkutil::TransitionImage(cmd, m_renderImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::TransitionImage(cmd, m_device.m_swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vkutil::CopyImageToImage(cmd, m_renderImage.image, m_device.m_swapchainImages[swapchainImageIndex], m_renderExtent, m_device.m_swapchainExtent);

    // set swapchain image layout to Attachment Optimal so we can draw it
    vkutil::TransitionImage(cmd, m_device.m_swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    //draw imgui into the swapchain image
    RenderImGui(cmd, m_device.m_swapchainImageViews[swapchainImageIndex]);

    // set swapchain image layout to Present so we can draw it
    vkutil::TransitionImage(cmd, m_device.m_swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

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
    VK_CHECK(vkQueueSubmit2(m_device.m_graphicsQueue, 1, &submit, GetCurrentFrame().renderFence));

    // Prepare present
    // This will put the image we just rendered to into the visible window.
    // We want to wait on the _renderSemaphore for that, 
    // as its necessary that drawing commands have finished before the image is displayed to the user
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.pSwapchains = &m_device.m_swapchain;
    presentInfo.swapchainCount = 1;

    presentInfo.pWaitSemaphores = &GetCurrentFrame().renderSemaphore;
    presentInfo.waitSemaphoreCount = 1;

    presentInfo.pImageIndices = &swapchainImageIndex;

    VkResult presentResult = vkQueuePresentKHR(m_device.m_graphicsQueue, &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR) 
    {
        m_resizeRequested = true;
    }

    m_frameIdx = (m_frameIdx + 1) % kFrameCount;

    HotReloadPipelines();
}

void Engine::Run()
{
    SDL_Event e;
    bool quit = false;

    // Main loop
    while (!quit)
    {
        m_input.m_mouseDeltaPos = { 0.0f, 0.0f };
        m_input.m_scroll = 0.0f;

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
                if (e.window.event == SDL_WINDOWEVENT_RESIZED)
                {
                    ResizeSwapchain(); 
                }
            }

            m_input.ProcessEvent(e);
            ImGui_ImplSDL2_ProcessEvent(&e);
        }

        if (m_input.IsKeyReleased(KeyboardKey_RightShift))
        {
            __debugbreak();
        }

        m_scene.Update(1.0f);

        if (!m_editor.m_update)
            m_camera.Input();

        // Do not render if we are minimized
        if (m_stopRendering) 
        {
            // Throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        if (m_resizeRequested) 
            ResizeSwapchain();

        // imgui new frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();

        ImGui::NewFrame();

        m_editor.Update();

        ImGui::Render();

        Render();

        m_input.UpdatePrevious();
    }
}

void Engine::ImmediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function)
{
    VK_CHECK(vkResetFences(m_device.m_device, 1, &m_immFence));
    VK_CHECK(vkResetCommandBuffer(m_immCommandBuffer, 0));

    VkCommandBuffer cmd = m_immCommandBuffer;

    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    function(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);
    VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, nullptr, nullptr);

    // Submit command buffer to the queue and execute it.
    // _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(m_device.m_graphicsQueue, 1, &submit, m_immFence));

    VK_CHECK(vkWaitForFences(m_device.m_device, 1, &m_immFence, true, 9999999999));
}

//void Engine::InitSwapchain()
//{
//    CreateSwapchain(m_windowExtent.width, m_windowExtent.height);
//
//    CreateSwapchainImageView();
//
//    // Add to deletion queues
//    m_deletionQueue.Push([=]() 
//    {
//        DestroyImage(m_depthImage);
//        DestroyImage(m_renderImage);
//    });
//}

void Engine::InitCommands()
{
    // Create a command pool for commands submitted to the graphics queue.
    // We also want the pool to allow for resetting of individual command buffers
    VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(m_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (int i = 0; i < kFrameCount; i++) 
    {
        VK_CHECK(vkCreateCommandPool(m_device.m_device, &commandPoolInfo, nullptr, &m_frames[i].commandPool));

        // Allocate the default command buffer that we will use for rendering
        VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(m_frames[i].commandPool, 1);

        VK_CHECK(vkAllocateCommandBuffers(m_device.m_device, &cmdAllocInfo, &m_frames[i].commandBuffer));
    }

    VK_CHECK(vkCreateCommandPool(m_device.m_device, &commandPoolInfo, nullptr, &m_immCommandPool));

    // allocate the command buffer for immediate submits
    VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(m_immCommandPool, 1);

    VK_CHECK(vkAllocateCommandBuffers(m_device.m_device, &cmdAllocInfo, &m_immCommandBuffer));

    m_deletionQueue.Push([=]() 
    {
        vkDestroyCommandPool(m_device.m_device, m_immCommandPool, nullptr);
    });
}

void Engine::InitSyncStructures()
{
    // Create syncronization structures
    // One fence to control when the gpu has finished rendering the frame,
    // and 2 semaphores to syncronize rendering with swapchain
    // We want the fence to start signalled so we can wait on it on the first frame
    VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

    for (int i = 0; i < kFrameCount; i++) 
    {
        VK_CHECK(vkCreateFence(m_device.m_device, &fenceCreateInfo, nullptr, &m_frames[i].renderFence));
        VK_CHECK(vkCreateSemaphore(m_device.m_device, &semaphoreCreateInfo, nullptr, &m_frames[i].swapchainSemaphore));
        VK_CHECK(vkCreateSemaphore(m_device.m_device, &semaphoreCreateInfo, nullptr, &m_frames[i].renderSemaphore));
    }

    VK_CHECK(vkCreateFence(m_device.m_device, &fenceCreateInfo, nullptr, &m_immFence));
    m_deletionQueue.Push([=]() { vkDestroyFence(m_device.m_device, m_immFence, nullptr); });
}

void Engine::InitDescriptors()
{
    // Create a descriptor pool that will hold 10 sets with 1 image each
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes =
    {
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }
    };

    m_globalDescriptorAllocator.Init(m_device.m_device, 10, sizes);

    // Make the descriptor set layout for our compute draw
    {
    DescriptorLayoutBuilder builder;
    builder.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    m_renderImageDescriptorLayout = builder.Build(m_device.m_device, VK_SHADER_STAGE_COMPUTE_BIT);
    }
    {
    DescriptorLayoutBuilder builder;
    builder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    m_gpuSceneDataDescriptorLayout = builder.Build(m_device.m_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    }
    {
    DescriptorLayoutBuilder builder;
    builder.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    builder.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    builder.AddBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    builder.AddBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    builder.AddBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    m_singleImageDescriptorLayout = builder.Build(m_device.m_device, VK_SHADER_STAGE_FRAGMENT_BIT);
    }
    // Allocate a descriptor set for our draw image
    m_renderImageDescriptors = m_globalDescriptorAllocator.Allocate(m_device.m_device, m_renderImageDescriptorLayout);

    {
    DescriptorWriter writer;
    writer.WriteImage(0, m_renderImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    writer.UpdateSet(m_device.m_device, m_renderImageDescriptors);
    }

    //make sure both the descriptor allocator and the new layout get cleaned up properly
    m_deletionQueue.Push([&]() 
    {
        m_globalDescriptorAllocator.DestroyPools(m_device.m_device);
        vkDestroyDescriptorSetLayout(m_device.m_device, m_renderImageDescriptorLayout, nullptr);
        vkDestroyDescriptorSetLayout(m_device.m_device, m_singleImageDescriptorLayout, nullptr);
        vkDestroyDescriptorSetLayout(m_device.m_device, m_gpuSceneDataDescriptorLayout, nullptr);
    });

    for (int i = 0; i < kFrameCount; i++) 
    {
        // create a descriptor pool
        std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frame_sizes = 
        {
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
        };

        m_frames[i].descriptors = DescriptorAllocatorGrowable{};
        m_frames[i].descriptors.Init(m_device.m_device, 1000, frame_sizes);

        m_deletionQueue.Push([&, i]() 
        {
            m_frames[i].descriptors.DestroyPools(m_device.m_device);
        });
    }
}

void Engine::InitPipelines()
{
	m_pipelines.push_back(new trayser::BackgroundPipeline());
	m_pipelines.push_back(new trayser::PBRPipeline());

	for (auto& pipeline : m_pipelines)
	{
		pipeline->Init();
	}
}

void Engine::InitImGui()
{
    // 1: Create descriptor pool for IMGUI
    // The size of the pool is very oversize, but it's copied from imgui demo
    // itself.
    VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    VkDescriptorPool imguiPool;
    VK_CHECK(vkCreateDescriptorPool(m_device.m_device, &pool_info, nullptr, &imguiPool));

    // 2: initialize imgui library

    // this initializes the core structures of imgui
    ImGui::CreateContext();

    // this initializes imgui for SDL
    ImGui_ImplSDL2_InitForVulkan(m_device.m_window);

    // this initializes imgui for Vulkan
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = m_device.m_instance;
    init_info.PhysicalDevice = m_device.m_physDevice;
    init_info.Device = m_device.m_device;
    init_info.Queue = m_device.m_graphicsQueue;
    init_info.DescriptorPool = imguiPool;
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    init_info.UseDynamicRendering = true;

    //dynamic rendering parameters for imgui to use
    init_info.PipelineRenderingCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &m_device.m_swapchainImageFormat;


    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info);

    ImGui_ImplVulkan_CreateFontsTexture();

    // add the destroy the imgui created structures
    m_deletionQueue.Push([=]() {
        ImGui_ImplVulkan_Shutdown();
        vkDestroyDescriptorPool(m_device.m_device, imguiPool, nullptr);
        });

    ImGuiStyle* style = &ImGui::GetStyle();

    style->WindowPadding = ImVec2(15, 15);
    style->WindowRounding = 5.0f;
    style->FramePadding = ImVec2(5, 5);
    style->FrameRounding = 4.0f;
    style->ItemSpacing = ImVec2(12, 8);
    style->ItemInnerSpacing = ImVec2(8, 6);
    style->IndentSpacing = 25.0f;
    style->ScrollbarSize = 15.0f;
    style->ScrollbarRounding = 9.0f;
    style->GrabMinSize = 5.0f;
    style->GrabRounding = 3.0f;

    style->Colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 0.83f, 1.00f);
    style->Colors[ImGuiCol_TextDisabled] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
    style->Colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
    style->Colors[ImGuiCol_PopupBg] = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
    style->Colors[ImGuiCol_Border] = ImVec4(0.80f, 0.80f, 0.83f, 0.88f);
    style->Colors[ImGuiCol_BorderShadow] = ImVec4(0.92f, 0.91f, 0.88f, 0.00f);
    style->Colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
    style->Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
    style->Colors[ImGuiCol_FrameBgActive] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
    style->Colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
    style->Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.00f, 0.98f, 0.95f, 0.75f);
    style->Colors[ImGuiCol_TitleBgActive] = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
    style->Colors[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
    style->Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
    style->Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
    style->Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
    style->Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
    style->Colors[ImGuiCol_CheckMark] = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
    style->Colors[ImGuiCol_SliderGrab] = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
    style->Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
    style->Colors[ImGuiCol_Button] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
    style->Colors[ImGuiCol_ButtonHovered] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
    style->Colors[ImGuiCol_ButtonActive] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
    style->Colors[ImGuiCol_Header] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
    style->Colors[ImGuiCol_HeaderHovered] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
    style->Colors[ImGuiCol_HeaderActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
    style->Colors[ImGuiCol_ResizeGrip] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style->Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
    style->Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
    style->Colors[ImGuiCol_PlotLines] = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
    style->Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
    style->Colors[ImGuiCol_PlotHistogram] = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
    style->Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
    style->Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.45f, 0.45f, 0.00f, 0.43f);
    style->Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.72f, 0.75f, 0.10f, 0.78f);
    style->Colors[ImGuiCol_SeparatorActive] = ImVec4(0.70f, 0.75f, 0.10f, 1.00f);
    style->Colors[ImGuiCol_Tab] = ImVec4(0.37f, 0.08f, 0.08f, 0.86f);
    style->Colors[ImGuiCol_TabHovered] = ImVec4(0.87f, 0.00f, 0.00f, 0.80f);
    style->Colors[ImGuiCol_TabActive] = ImVec4(0.87f, 0.00f, 0.00f, 1.00f);
    style->Colors[ImGuiCol_TabUnfocused] = ImVec4(0.75f, 0.00f, 0.00f, 0.97f);
    style->Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.61f, 0.00f, 0.00f, 1.00f);
}

void Engine::InitDefaultData()
{
    //3 default textures, white, grey, black. 1 pixel each
    uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
    m_whiteImage = CreateImage((void*)&white, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT);

    uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));
    m_greyImage = CreateImage((void*)&grey, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT);

    uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
    m_blackImage = CreateImage((void*)&black, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT);

    InitDefaultMaterial();

    VkSamplerCreateInfo sampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };

    sampl.magFilter = VK_FILTER_LINEAR;
    sampl.minFilter = VK_FILTER_LINEAR;
    vkCreateSampler(m_device.m_device, &sampl, nullptr, &m_defaultSamplerNearest);

    sampl.magFilter = VK_FILTER_LINEAR;
    sampl.minFilter = VK_FILTER_LINEAR;
    vkCreateSampler(m_device.m_device, &sampl, nullptr, &m_defaultSamplerLinear);

    m_deletionQueue.Push([&]() 
    {
        vkDestroySampler(m_device.m_device, m_defaultSamplerNearest, nullptr);
        vkDestroySampler(m_device.m_device, m_defaultSamplerLinear, nullptr);

        DestroyImage(m_whiteImage);
        DestroyImage(m_greyImage);
        DestroyImage(m_blackImage);
    });
}

void Engine::InitDefaultMaterial()
{
    uint32_t black  = glm::packUnorm4x8(glm::vec4(0.0, 0.0, 0.0, 1.0));
    uint32_t gray   = glm::packUnorm4x8(glm::vec4(0.5, 0.5, 0.5, 1.0));
    uint32_t normal = glm::packUnorm4x8(glm::vec4(0.5, 0.5, 1.0, 1.0));
    uint32_t white  = glm::packUnorm4x8(glm::vec4(1.0, 1.0, 1.0, 1.0));

    m_defaultMaterial.baseColor         = m_resources.Create<Image>("default_base_color_image", &black, 1, 1, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_SAMPLED_BIT);
    m_defaultMaterial.metallicRoughness = m_resources.Create<Image>("default_metallic_roughness_image", &gray, 1, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
    m_defaultMaterial.emissive          = m_resources.Create<Image>("default_emissive_image", &black, 1, 1, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_SAMPLED_BIT);
    m_defaultMaterial.occlusion         = m_resources.Create<Image>("default_ambient_occlusion_image", &white, 1, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
    m_defaultMaterial.normalMap         = m_resources.Create<Image>("default_normal_map_image", &normal, 1, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
}

void Engine::CreateSwapchainImageView()
{
    // Draw image size will match the window
    VkExtent3D drawImageExtent =
    {
        m_windowExtent.width,
        m_windowExtent.height,
        1
    };

    // Hardcoding the render format to 32 bit float
    m_renderImage.imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
    m_renderImage.imageExtent = drawImageExtent;

    VkImageUsageFlags drawImageUsages{};
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImageCreateInfo rimg_info = vkinit::image_create_info(m_renderImage.imageFormat, drawImageUsages, drawImageExtent);

    // For the draw image, we want to allocate it from gpu local memory
    VmaAllocationCreateInfo rimg_allocinfo = {};
    rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // Allocate and create the image
    vmaCreateImage(m_allocator, &rimg_info, &rimg_allocinfo, &m_renderImage.image, &m_renderImage.allocation, nullptr);

    // Build a image-view for the draw image to use for rendering
    VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(m_renderImage.imageFormat, m_renderImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

    VK_CHECK(vkCreateImageView(m_device.m_device, &rview_info, nullptr, &m_renderImage.imageView));
    
    // Depth image
    m_depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
    m_renderImage.imageExtent = drawImageExtent;

    VkImageUsageFlags depthImageUsages{};
    depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    VkImageCreateInfo dimg_info = vkinit::image_create_info(m_depthImage.imageFormat, depthImageUsages, drawImageExtent);
    vmaCreateImage(m_allocator, &dimg_info, &rimg_allocinfo, &m_depthImage.image, &m_depthImage.allocation, nullptr);
    VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(m_depthImage.imageFormat, m_depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);
    VK_CHECK(vkCreateImageView(m_device.m_device, &dview_info, nullptr, &m_depthImage.imageView));
}

AllocatedBuffer Engine::CreateBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
{
    // allocate buffer
    VkBufferCreateInfo bufferInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.pNext = nullptr;
    bufferInfo.size = allocSize;

    bufferInfo.usage = usage;

    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage = memoryUsage;
    vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    AllocatedBuffer newBuffer;

    // allocate the buffer
    VK_CHECK(vmaCreateBuffer(m_allocator, &bufferInfo, &vmaallocInfo, &newBuffer.buffer, &newBuffer.allocation,
        &newBuffer.info));

    return newBuffer;
}

AllocatedImage Engine::CreateImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
    AllocatedImage newImage;
    newImage.imageFormat = format;
    newImage.imageExtent = size;

    VkImageCreateInfo img_info = vkinit::image_create_info(format, usage, size);
    if (mipmapped) {
        img_info.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
    }

    // always allocate images on dedicated GPU memory
    VmaAllocationCreateInfo allocinfo = {};
    allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // allocate and create the image
    VK_CHECK(vmaCreateImage(m_allocator, &img_info, &allocinfo, &newImage.image, &newImage.allocation, nullptr));

    // if the format is a depth format, we will need to have it use the correct
    // aspect flag
    VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
    if (format == VK_FORMAT_D32_SFLOAT) {
        aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    // build a image-view for the image
    VkImageViewCreateInfo view_info = vkinit::imageview_create_info(format, newImage.image, aspectFlag);
    view_info.subresourceRange.levelCount = img_info.mipLevels;

    VK_CHECK(vkCreateImageView(m_device.m_device, &view_info, nullptr, &newImage.imageView));

    return newImage;
}

AllocatedImage Engine::CreateImage(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
    size_t data_size = size.depth * size.width * size.height * 4;
    AllocatedBuffer uploadbuffer = CreateBuffer(data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    memcpy(uploadbuffer.info.pMappedData, data, data_size);

    AllocatedImage new_image = CreateImage(size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped);

    ImmediateSubmit([&](VkCommandBuffer cmd) {
        vkutil::TransitionImage(cmd, new_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkBufferImageCopy copyRegion = {};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;

        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent = size;

        // copy the buffer into the image
        vkCmdCopyBufferToImage(cmd, uploadbuffer.buffer, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
            &copyRegion);

        vkutil::TransitionImage(cmd, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        });

    DestroyBuffer(uploadbuffer);

    return new_image;
}

void Engine::DestroyImage(const AllocatedImage& img)
{
    vkDestroyImageView(m_device.m_device, img.imageView, nullptr);
    vmaDestroyImage(m_allocator, img.image, img.allocation);
}

gpu::MeshBuffers Engine::UploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices)
{
    const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

    gpu::MeshBuffers newSurface;

    //create vertex buffer
    newSurface.vertexBuffer = CreateBuffer(vertexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    //find the adress of the vertex buffer
    VkBufferDeviceAddressInfo deviceAdressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,.buffer = newSurface.vertexBuffer.buffer };
    newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(m_device.m_device, &deviceAdressInfo);

    //create index buffer
    newSurface.indexBuffer = CreateBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    AllocatedBuffer staging = CreateBuffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    VmaAllocationInfo info;
    vmaGetAllocationInfo(m_allocator, staging.allocation, &info);
    void* data = info.pMappedData; // safe to use

    // copy vertex buffer
    memcpy(data, vertices.data(), vertexBufferSize);
    // copy index buffer
    memcpy((char*)data + vertexBufferSize, indices.data(), indexBufferSize);

    ImmediateSubmit([&](VkCommandBuffer cmd) {
        VkBufferCopy vertexCopy{ 0 };
        vertexCopy.dstOffset = 0;
        vertexCopy.srcOffset = 0;
        vertexCopy.size = vertexBufferSize;

        vkCmdCopyBuffer(cmd, staging.buffer, newSurface.vertexBuffer.buffer, 1, &vertexCopy);

        VkBufferCopy indexCopy{ 0 };
        indexCopy.dstOffset = 0;
        indexCopy.srcOffset = vertexBufferSize;
        indexCopy.size = indexBufferSize;

        vkCmdCopyBuffer(cmd, staging.buffer, newSurface.indexBuffer.buffer, 1, &indexCopy);
        });

    DestroyBuffer(staging);

    return newSurface;
}

void Engine::DestroyBuffer(const AllocatedBuffer& buffer)
{
    vmaDestroyBuffer(m_allocator, buffer.buffer, buffer.allocation);
}

void Engine::HotReloadPipelines()
{
    for (int i = 0; i < m_pipelines.size(); i++)
    {
		m_pipelines[i]->ReloadIfChanged();
    }
}

void Engine::DestroySwapchain()
{
    vkDestroySwapchainKHR(m_device.m_device, m_device.m_swapchain, nullptr);

    for (int i = 0; i < m_device.m_swapchainImageViews.size(); i++)
    {
        vkDestroyImageView(m_device.m_device, m_device.m_swapchainImageViews[i], nullptr);
    }
}

void Engine::BeginRecording(VkCommandBuffer cmd)
{
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));
}

void Engine::RenderImGui(VkCommandBuffer cmd, VkImageView targetImageView)
{
    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingInfo renderInfo = vkinit::rendering_info(m_renderExtent, &colorAttachment, nullptr);

    vkCmdBeginRendering(cmd, &renderInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    vkCmdEndRendering(cmd);
}

void Engine::ResizeSwapchain()
{
    vkDeviceWaitIdle(m_device.m_device);

    DestroySwapchain();

    int w, h;
    SDL_GetWindowSize(m_device.m_window, &w, &h);
    m_windowExtent.width = w;
    m_windowExtent.height = h;

    m_renderImage.imageExtent = { m_windowExtent.width, m_windowExtent.height, 1 };
    m_renderExtent = { m_windowExtent.width, m_windowExtent.height };

    // TODO
    //CreateSwapchain(m_windowExtent.width, m_windowExtent.height);

    vkDestroyImageView(m_device.m_device, m_renderImage.imageView, nullptr);
    vmaDestroyImage(m_allocator, m_renderImage.image, m_renderImage.allocation);

    CreateSwapchainImageView();

    m_resizeRequested = false;
}