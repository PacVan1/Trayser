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

//#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

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
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    SDL_ShowCursor(SDL_DISABLE);
    SDL_SetRelativeMouseMode(SDL_FALSE);

    InitVulkan();
    InitSwapchain();
    InitCommands();
    InitSyncStructures();   
    InitDescriptors();
    InitPipelines();
    InitImGui();
    InitDefaultData();

    Model::MikkTSpaceInit();

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
            m_frames[i].deletionQueue.Flush();
        }

        m_deletionQueue.Flush();

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
    GetCurrentFrame().deletionQueue.Flush();
    GetCurrentFrame().descriptors.ClearPools(m_device);

    VK_CHECK(vkResetFences(m_device, 1, &GetCurrentFrame().renderFence));

    uint32_t swapchainImageIndex;
    VkResult e = vkAcquireNextImageKHR(m_device, m_swapchain, 1000000000, GetCurrentFrame().swapchainSemaphore, nullptr, &swapchainImageIndex);
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
    vkutil::TransitionImage(cmd, m_swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    
    RenderBackground(cmd);

    vkutil::TransitionImage(cmd, m_renderImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vkutil::TransitionImage(cmd, m_depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    RenderTriangle(cmd);

    // Transition the draw image and the swapchain image into their correct transfer layouts
    vkutil::TransitionImage(cmd, m_renderImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::TransitionImage(cmd, m_swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vkutil::CopyImageToImage(cmd, m_renderImage.image, m_swapchainImages[swapchainImageIndex], m_renderExtent, m_swapchainExtent);

    // set swapchain image layout to Attachment Optimal so we can draw it
    vkutil::TransitionImage(cmd, m_swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    //draw imgui into the swapchain image
    RenderImGui(cmd, m_swapchainImageViews[swapchainImageIndex]);

    // set swapchain image layout to Present so we can draw it
    vkutil::TransitionImage(cmd, m_swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

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

    VkResult presentResult = vkQueuePresentKHR(m_graphicsQueue, &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR) 
    {
        m_resizeRequested = true;
    }

    m_frameIdx = (m_frameIdx + 1) % kFrameCount;
}

void VulkanEngine::Run()
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

void VulkanEngine::ImmediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function)
{
    VK_CHECK(vkResetFences(m_device, 1, &m_immFence));
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
    VK_CHECK(vkQueueSubmit2(m_graphicsQueue, 1, &submit, m_immFence));

    VK_CHECK(vkWaitForFences(m_device, 1, &m_immFence, true, 9999999999));
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

    // Initialize VMA
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice    = m_chosenGPU;
    allocatorInfo.device            = m_device;
    allocatorInfo.instance          = m_instance;
    allocatorInfo.flags             = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &m_allocator);

    m_deletionQueue.Push([&]() { vmaDestroyAllocator(m_allocator); });
}

void VulkanEngine::InitSwapchain()
{
    CreateSwapchain(m_windowExtent.width, m_windowExtent.height);

    CreateSwapchainImageView();

    // Add to deletion queues
    m_deletionQueue.Push([=]() 
    {
        DestroyImage(m_depthImage);
        DestroyImage(m_renderImage);
    });
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

    VK_CHECK(vkCreateCommandPool(m_device, &commandPoolInfo, nullptr, &m_immCommandPool));

    // allocate the command buffer for immediate submits
    VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(m_immCommandPool, 1);

    VK_CHECK(vkAllocateCommandBuffers(m_device, &cmdAllocInfo, &m_immCommandBuffer));

    m_deletionQueue.Push([=]() 
    {
        vkDestroyCommandPool(m_device, m_immCommandPool, nullptr);
    });
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

    VK_CHECK(vkCreateFence(m_device, &fenceCreateInfo, nullptr, &m_immFence));
    m_deletionQueue.Push([=]() { vkDestroyFence(m_device, m_immFence, nullptr); });
}

void VulkanEngine::InitDescriptors()
{
    // Create a descriptor pool that will hold 10 sets with 1 image each
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes =
    {
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }
    };

    m_globalDescriptorAllocator.Init(m_device, 10, sizes);

    // Make the descriptor set layout for our compute draw
    {
    DescriptorLayoutBuilder builder;
    builder.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    m_renderImageDescriptorLayout = builder.Build(m_device, VK_SHADER_STAGE_COMPUTE_BIT);
    }
    {
    DescriptorLayoutBuilder builder;
    builder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    m_gpuSceneDataDescriptorLayout = builder.Build(m_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    }
    {
    DescriptorLayoutBuilder builder;
    builder.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    builder.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    builder.AddBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    builder.AddBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    builder.AddBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    m_singleImageDescriptorLayout = builder.Build(m_device, VK_SHADER_STAGE_FRAGMENT_BIT);
    }
    // Allocate a descriptor set for our draw image
    m_renderImageDescriptors = m_globalDescriptorAllocator.Allocate(m_device, m_renderImageDescriptorLayout);

    {
    DescriptorWriter writer;
    writer.WriteImage(0, m_renderImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    writer.UpdateSet(m_device, m_renderImageDescriptors);
    }

    //make sure both the descriptor allocator and the new layout get cleaned up properly
    m_deletionQueue.Push([&]() 
    {
        m_globalDescriptorAllocator.DestroyPools(m_device);
        vkDestroyDescriptorSetLayout(m_device, m_renderImageDescriptorLayout, nullptr);
        vkDestroyDescriptorSetLayout(m_device, m_singleImageDescriptorLayout, nullptr);
        vkDestroyDescriptorSetLayout(m_device, m_gpuSceneDataDescriptorLayout, nullptr);
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
        m_frames[i].descriptors.Init(m_device, 1000, frame_sizes);

        m_deletionQueue.Push([&, i]() 
        {
            m_frames[i].descriptors.DestroyPools(m_device);
        });
    }
}

void VulkanEngine::InitPipelines()
{
    InitBackgroundPipelines();
    InitTrianglePipelines();
    InitMeshPipelines();
}

void VulkanEngine::InitBackgroundPipelines()
{
    VkPipelineLayoutCreateInfo computeLayout{};
    computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    computeLayout.pNext = nullptr;
    computeLayout.pSetLayouts = &m_renderImageDescriptorLayout;
    computeLayout.setLayoutCount = 1;

    VkPushConstantRange pushConstant{};
    pushConstant.offset = 0;
    pushConstant.size = sizeof(ComputePushConstants);
    pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    computeLayout.pPushConstantRanges = &pushConstant;
    computeLayout.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(m_device, &computeLayout, nullptr, &m_gradientPipelineLayout));

    // Layout code
    VkShaderModule computeDrawShader;
    if (!vkutil::LoadShaderModule("../shaders/sky.comp.spv", m_device, &computeDrawShader))
    {
        fmt::print("Error when building the compute shader \n");
    }

    VkPipelineShaderStageCreateInfo stageinfo{};
    stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageinfo.pNext = nullptr;
    stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageinfo.module = computeDrawShader;
    stageinfo.pName = "main";

    VkComputePipelineCreateInfo computePipelineCreateInfo{};
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.pNext = nullptr;
    computePipelineCreateInfo.layout = m_gradientPipelineLayout;
    computePipelineCreateInfo.stage = stageinfo;

    VK_CHECK(vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &m_gradientPipeline));

    vkDestroyShaderModule(m_device, computeDrawShader, nullptr);

    m_deletionQueue.Push([&]() 
    {
        vkDestroyPipelineLayout(m_device, m_gradientPipelineLayout, nullptr);
        vkDestroyPipeline(m_device, m_gradientPipeline, nullptr);
    });
}

void VulkanEngine::InitTrianglePipelines()
{
    VkShaderModule triangleFragShader;
    if (!vkutil::LoadShaderModule("../shaders/colored_triangle.frag.spv", m_device, &triangleFragShader))
    {
        fmt::print("Error when building the triangle fragment shader module");
    }
    else {
        fmt::print("Triangle fragment shader succesfully loaded");
    }

    VkShaderModule triangleVertexShader;
    if (!vkutil::LoadShaderModule("../shaders/colored_triangle.vert.spv", m_device, &triangleVertexShader)) 
    {
        fmt::print("Error when building the triangle vertex shader module");
    }
    else {
        fmt::print("Triangle vertex shader succesfully loaded");
    }

    VkPushConstantRange bufferRange{};
    bufferRange.offset = 0;
    bufferRange.size = sizeof(gpu::RenderPushConstants);
    bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
    pipeline_layout_info.pPushConstantRanges = &bufferRange;
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pSetLayouts = &m_singleImageDescriptorLayout;
    pipeline_layout_info.setLayoutCount = 1;
    VK_CHECK(vkCreatePipelineLayout(m_device, &pipeline_layout_info, nullptr, &m_meshPipelineLayout));

    //build the pipeline layout that controls the inputs/outputs of the shader
    //we are not using descriptor sets or other systems yet, so no need to use anything other than empty default
    VK_CHECK(vkCreatePipelineLayout(m_device, &pipeline_layout_info, nullptr, &m_triPipelineLayout));

    vkutil::PipelineBuilder pipelineBuilder;

    //use the triangle layout we created
    pipelineBuilder.m_pipelineLayout = m_triPipelineLayout;
    //connecting the vertex and pixel shaders to the pipeline
    pipelineBuilder.SetShaders(triangleVertexShader, triangleFragShader);
    //it will draw triangles
    pipelineBuilder.SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    //filled triangles
    pipelineBuilder.SetPolygonMode(VK_POLYGON_MODE_FILL);
    //no backface culling
    pipelineBuilder.SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    //no multisampling
    pipelineBuilder.SetMultisamplingNone();
    //no blending
    pipelineBuilder.DisableBlending();
    //no depth testing
    pipelineBuilder.DisableDepthTest();

    //connect the image format we will draw into, from draw image
    pipelineBuilder.SetColorAttachmentFormat(m_renderImage.imageFormat);
    pipelineBuilder.SetDepthFormat(VK_FORMAT_UNDEFINED);

    //finally build the pipeline
    m_triPipeline = pipelineBuilder.Build(m_device);

    //clean structures
    vkDestroyShaderModule(m_device, triangleFragShader, nullptr);
    vkDestroyShaderModule(m_device, triangleVertexShader, nullptr);

    m_deletionQueue.Push([&]() 
    {
        vkDestroyPipelineLayout(m_device, m_triPipelineLayout, nullptr);
        vkDestroyPipeline(m_device, m_triPipeline, nullptr);
    });
}

void VulkanEngine::InitMeshPipelines()
{
    VkShaderModule triangleFragShader;
    if (!vkutil::LoadShaderModule("../shaders/triangle.frag.spv", m_device, &triangleFragShader)) {
        fmt::print("Error when building the triangle fragment shader module");
    }
    else {
        fmt::print("Triangle fragment shader succesfully loaded");
    }

    VkShaderModule triangleVertexShader;
    if (!vkutil::LoadShaderModule("../shaders/triangle.vert.spv", m_device, &triangleVertexShader)) {
        fmt::print("Error when building the triangle vertex shader module");
    }
    else {
        fmt::print("Triangle vertex shader succesfully loaded");
    }

    VkPushConstantRange bufferRange{};
    bufferRange.offset = 0;
    bufferRange.size = sizeof(gpu::RenderPushConstants);
    bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPushConstantRange fragPushConst{};
    fragPushConst.offset = sizeof(gpu::RenderPushConstants);
    fragPushConst.size = sizeof(gpu::RenderPushConstantsFrag);
    fragPushConst.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkPushConstantRange pushConsts[2] = { bufferRange, fragPushConst };

    VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
    pipeline_layout_info.pPushConstantRanges = pushConsts;
    pipeline_layout_info.pushConstantRangeCount = 2;
    pipeline_layout_info.pSetLayouts = &m_singleImageDescriptorLayout;
    pipeline_layout_info.setLayoutCount = 1;
    VK_CHECK(vkCreatePipelineLayout(m_device, &pipeline_layout_info, nullptr, &m_meshPipelineLayout));

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    auto bindingDescription = Vertex::GetBindingDescription();
    auto attributeDescriptions = Vertex::GetAttributeDescriptions();
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription; // Optional
    vertexInputInfo.vertexAttributeDescriptionCount = attributeDescriptions.size();
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data(); // Optional

    vkutil::PipelineBuilder pipelineBuilder;

    //use the triangle layout we created
    pipelineBuilder.m_pipelineLayout = m_meshPipelineLayout;
    //connecting the vertex and pixel shaders to the pipeline
    pipelineBuilder.SetShaders(triangleVertexShader, triangleFragShader);
    //it will draw triangles
    pipelineBuilder.SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    //vertex input info
    pipelineBuilder.SetVertexInputInfo(&vertexInputInfo);
    //filled triangles
    pipelineBuilder.SetPolygonMode(VK_POLYGON_MODE_FILL);
    //no backface culling
    pipelineBuilder.SetCullMode(VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_CLOCKWISE);
    //no multisampling
    pipelineBuilder.SetMultisamplingNone();
    //no blending
    pipelineBuilder.DisableBlending();

    pipelineBuilder.EnableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

    //connect the image format we will draw into, from draw image
    pipelineBuilder.SetColorAttachmentFormat(m_renderImage.imageFormat);
    pipelineBuilder.SetDepthFormat(m_depthImage.imageFormat);

    //finally build the pipeline
    m_meshPipeline = pipelineBuilder.Build(m_device);

    //clean structures
    vkDestroyShaderModule(m_device, triangleFragShader, nullptr);
    vkDestroyShaderModule(m_device, triangleVertexShader, nullptr);

    m_deletionQueue.Push([&]()
    {
        vkDestroyPipelineLayout(m_device, m_meshPipelineLayout, nullptr);
        vkDestroyPipeline(m_device, m_meshPipeline, nullptr);
    });
}

void VulkanEngine::InitImGui()
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
    VK_CHECK(vkCreateDescriptorPool(m_device, &pool_info, nullptr, &imguiPool));

    // 2: initialize imgui library

    // this initializes the core structures of imgui
    ImGui::CreateContext();

    // this initializes imgui for SDL
    ImGui_ImplSDL2_InitForVulkan(m_window);

    // this initializes imgui for Vulkan
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = m_instance;
    init_info.PhysicalDevice = m_chosenGPU;
    init_info.Device = m_device;
    init_info.Queue = m_graphicsQueue;
    init_info.DescriptorPool = imguiPool;
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    init_info.UseDynamicRendering = true;

    //dynamic rendering parameters for imgui to use
    init_info.PipelineRenderingCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &m_swapchainImageFormat;


    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info);

    ImGui_ImplVulkan_CreateFontsTexture();

    // add the destroy the imgui created structures
    m_deletionQueue.Push([=]() {
        ImGui_ImplVulkan_Shutdown();
        vkDestroyDescriptorPool(m_device, imguiPool, nullptr);
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

void VulkanEngine::InitDefaultData()
{
    std::array<Vertex, 4> rect_vertices;

    rect_vertices[0].position = { 0.5,-0.5, 0 };
    rect_vertices[1].position = { 0.5,0.5, 0 };
    rect_vertices[2].position = { -0.5,-0.5, 0 };
    rect_vertices[3].position = { -0.5,0.5, 0 };

    rect_vertices[0].color = { 0,0, 0,1 };
    rect_vertices[1].color = { 0.5,0.5,0.5 ,1 };
    rect_vertices[2].color = { 1,0, 0,1 };
    rect_vertices[3].color = { 0,1, 0,1 };

    std::array<uint32_t, 6> rect_indices;

    rect_indices[0] = 0;
    rect_indices[1] = 1;
    rect_indices[2] = 2;

    rect_indices[3] = 2;
    rect_indices[4] = 1;
    rect_indices[5] = 3;

    rectangle = UploadMesh(rect_indices, rect_vertices);

    //delete the rectangle data on engine shutdown
    m_deletionQueue.Push([&]() 
    {
        DestroyBuffer(rectangle.indexBuffer);
        DestroyBuffer(rectangle.vertexBuffer);
    });

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
    vkCreateSampler(m_device, &sampl, nullptr, &m_defaultSamplerNearest);

    sampl.magFilter = VK_FILTER_LINEAR;
    sampl.minFilter = VK_FILTER_LINEAR;
    vkCreateSampler(m_device, &sampl, nullptr, &m_defaultSamplerLinear);

    m_deletionQueue.Push([&]() 
    {
        vkDestroySampler(m_device, m_defaultSamplerNearest, nullptr);
        vkDestroySampler(m_device, m_defaultSamplerLinear, nullptr);

        DestroyImage(m_whiteImage);
        DestroyImage(m_greyImage);
        DestroyImage(m_blackImage);
    });
}

void VulkanEngine::InitDefaultMaterial()
{
    //checkerboard image
    uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
    uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
    std::array<uint32_t, 16 * 16 > pixels; //for 16x16 checkerboard texture
    for (int x = 0; x < 16; x++) 
    {
        for (int y = 0; y < 16; y++) 
        {
            pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
        }
    }

    m_defaultMaterial.baseColor         = m_resources.Create<Image>("default_base_color_image", pixels.data(), 16, 16, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_SAMPLED_BIT);
    m_defaultMaterial.metallicRoughness = m_resources.Create<Image>("default_metallic_roughness_image", pixels.data(), 16, 16, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
    m_defaultMaterial.emissive          = m_resources.Create<Image>("default_emissive_image", pixels.data(), 16, 16, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_SAMPLED_BIT);
    m_defaultMaterial.occlusion         = m_resources.Create<Image>("default_ambient_occlusion_image", pixels.data(), 16, 16, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
    m_defaultMaterial.normalMap         = m_resources.Create<Image>("default_normal_map_image", pixels.data(), 16, 16, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
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

void VulkanEngine::CreateSwapchainImageView()
{
    // Draw image size will match the window
    VkExtent3D drawImageExtent =
    {
        m_windowExtent.width,
        m_windowExtent.height,
        1
    };

    // Hardcoding the render format to 32 bit float
    m_renderImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
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

    VK_CHECK(vkCreateImageView(m_device, &rview_info, nullptr, &m_renderImage.imageView));
    
    // Depth image
    m_depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
    m_renderImage.imageExtent = drawImageExtent;

    VkImageUsageFlags depthImageUsages{};
    depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    VkImageCreateInfo dimg_info = vkinit::image_create_info(m_depthImage.imageFormat, depthImageUsages, drawImageExtent);
    vmaCreateImage(m_allocator, &dimg_info, &rimg_allocinfo, &m_depthImage.image, &m_depthImage.allocation, nullptr);
    VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(m_depthImage.imageFormat, m_depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);
    VK_CHECK(vkCreateImageView(m_device, &dview_info, nullptr, &m_depthImage.imageView));
}

AllocatedBuffer VulkanEngine::CreateBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
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

AllocatedImage VulkanEngine::CreateImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
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

    VK_CHECK(vkCreateImageView(m_device, &view_info, nullptr, &newImage.imageView));

    return newImage;
}

AllocatedImage VulkanEngine::CreateImage(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
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

void VulkanEngine::DestroyImage(const AllocatedImage& img)
{
    vkDestroyImageView(m_device, img.imageView, nullptr);
    vmaDestroyImage(m_allocator, img.image, img.allocation);
}

gpu::MeshBuffers VulkanEngine::UploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices)
{
    const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

    gpu::MeshBuffers newSurface;

    //create vertex buffer
    newSurface.vertexBuffer = CreateBuffer(vertexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    //find the adress of the vertex buffer
    VkBufferDeviceAddressInfo deviceAdressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,.buffer = newSurface.vertexBuffer.buffer };
    newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(m_device, &deviceAdressInfo);

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

void VulkanEngine::DestroyBuffer(const AllocatedBuffer& buffer)
{
    vmaDestroyBuffer(m_allocator, buffer.buffer, buffer.allocation);
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

void VulkanEngine::RenderBackground(VkCommandBuffer cmd)
{
    // bind the gradient drawing compute pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_gradientPipeline);

    // bind the descriptor set containing the draw image for the compute pipeline
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_gradientPipelineLayout, 0, 1, &m_renderImageDescriptors, 0, nullptr);

    ComputePushConstants pc;
    pc.data1 = glm::vec4(0.1, 0.2, 0.4, 0.97);

    vkCmdPushConstants(cmd, m_gradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &pc);

    // execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
    vkCmdDispatch(cmd, std::ceil(m_renderExtent.width / 16.0), std::ceil(m_renderExtent.height / 16.0), 1);
}

void VulkanEngine::RenderTriangle(VkCommandBuffer cmd)
{
    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(m_renderImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(m_depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkRenderingInfo renderInfo = vkinit::rendering_info(m_renderExtent, &colorAttachment, &depthAttachment);
    vkCmdBeginRendering(cmd, &renderInfo);

    //set dynamic viewport and scissor
    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = m_renderExtent.width;
    viewport.height = m_renderExtent.height;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;

    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = m_renderExtent.width;
    scissor.extent.height = m_renderExtent.height;

    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_meshPipeline);

    auto view = m_scene.m_registry.view<WorldTransform, RenderComponent>();
    for (const auto& [ent, tf, render] : view.each())
    {
        for (auto& prim : render.mesh->primitives)
        {
            //bind a texture
            VkDescriptorSet imageSet = GetCurrentFrame().descriptors.Allocate(m_device, m_singleImageDescriptorLayout);
            {
                DescriptorWriter writer;
                writer.WriteBindlessImage(0, render.mesh->materials[prim.materialId].baseColor ? render.mesh->materials[prim.materialId].baseColor->imageView : m_defaultMaterial.baseColor->imageView, m_defaultSamplerNearest, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                writer.WriteBindlessImage(1, render.mesh->materials[prim.materialId].normalMap ? render.mesh->materials[prim.materialId].normalMap->imageView : m_defaultMaterial.normalMap->imageView, m_defaultSamplerNearest, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                writer.WriteBindlessImage(2, render.mesh->materials[prim.materialId].metallicRoughness ? render.mesh->materials[prim.materialId].metallicRoughness->imageView : m_defaultMaterial.metallicRoughness->imageView, m_defaultSamplerNearest, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                writer.WriteBindlessImage(3, render.mesh->materials[prim.materialId].occlusion ? render.mesh->materials[prim.materialId].occlusion->imageView : m_defaultMaterial.occlusion->imageView, m_defaultSamplerNearest, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                writer.WriteBindlessImage(4, render.mesh->materials[prim.materialId].emissive ? render.mesh->materials[prim.materialId].emissive->imageView : m_defaultMaterial.emissive->imageView, m_defaultSamplerNearest, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

                writer.UpdateSet(m_device, imageSet);
            }

            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_meshPipelineLayout, 0, 1, &imageSet, 0, nullptr);

            gpu::RenderPushConstants vertPushConstants;
            glm::mat4 projection = m_camera.m_proj;
            projection[1][1] *= -1;

            gpu::RenderPushConstantsFrag fragPushConstants;
            fragPushConstants.renderMode = glm::ivec4(m_renderMode, 0, 0, 0);

            vertPushConstants.viewProj = projection * m_camera.m_view;
            vertPushConstants.model = tf.matrix;

            VkBuffer vertexBuffers[] = { render.mesh->vertexBuffer.buffer };
            VkDeviceSize offsets[] = { 0 };
            vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);

            vkCmdPushConstants(cmd, m_meshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(gpu::RenderPushConstants), &vertPushConstants);
            vkCmdPushConstants(cmd, m_meshPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(gpu::RenderPushConstants), sizeof(gpu::RenderPushConstantsFrag), &fragPushConstants);
            //vkCmdBindIndexBuffer(cmd, render.mesh->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

            //vkCmdDrawIndexed(cmd, render.mesh->surfaces[0].count, 1, render.mesh->surfaces[0].startIndex, 0, 0);
            vkCmdDraw(cmd, prim.vertexCount, 1, 0, 0);
        }
    }

    vkCmdEndRendering(cmd);
}

void VulkanEngine::RenderImGui(VkCommandBuffer cmd, VkImageView targetImageView)
{
    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingInfo renderInfo = vkinit::rendering_info(m_renderExtent, &colorAttachment, nullptr);

    vkCmdBeginRendering(cmd, &renderInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    vkCmdEndRendering(cmd);
}

void VulkanEngine::ResizeSwapchain()
{
    vkDeviceWaitIdle(m_device);

    DestroySwapchain();

    int w, h;
    SDL_GetWindowSize(m_window, &w, &h);
    m_windowExtent.width = w;
    m_windowExtent.height = h;

    m_renderImage.imageExtent = { m_windowExtent.width, m_windowExtent.height, 1 };
    m_renderExtent = { m_windowExtent.width, m_windowExtent.height };

    CreateSwapchain(m_windowExtent.width, m_windowExtent.height);

    vkDestroyImageView(m_device, m_renderImage.imageView, nullptr);
    vmaDestroyImage(m_allocator, m_renderImage.image, m_renderImage.allocation);

    CreateSwapchainImageView();

    m_resizeRequested = false;
}
