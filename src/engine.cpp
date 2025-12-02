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

namespace trayser 
{

Engine g_engine;

}

void trayser::Engine::Init()
{
    m_compiler.Init();
    m_device.Init();

    InitImGuiStyle();

    CreateSwapchainImageView();
    InitDescriptors();
    InitPipelines();
    InitDefaultData();

    Model::MikkTSpaceInit();

    auto model = m_resources.Create<Model>(kModelPaths[ModelResource_DamagedHelmet], kModelPaths[ModelResource_DamagedHelmet], this);
    m_scene.CreateModel(model);

    m_isInitialized = true;
}

void trayser::Engine::Cleanup()
{
    if (m_isInitialized) 
    {    
        // Make sure the gpu is done with its work
        vkDeviceWaitIdle(m_device.m_device);

        //for (int i = 0; i < kFrameCount; i++) 
        //{
        //    vkDestroyCommandPool(m_device.m_device, m_frames[i].commandPool, nullptr);
        //    vkDestroyFence(m_device.m_device, m_frames[i].renderFence, nullptr);
        //    vkDestroySemaphore(m_device.m_device, m_frames[i].renderSemaphore, nullptr);
        //    vkDestroySemaphore(m_device.m_device, m_frames[i].swapchainSemaphore, nullptr);
        //    m_frames[i].deletionQueue.Flush();
        //}

        m_deletionQueue.Flush();

        DestroySwapchain();

        vkDestroySurfaceKHR(m_device.m_instance, m_device.m_surface, nullptr);
        vkDestroyDevice(m_device.m_device, nullptr);

        //vkb::destroy_debug_utils_messenger(m_device.m_instance, m_device.m_debugMessenger);
        vkDestroyInstance(m_device.m_instance, nullptr);
        SDL_DestroyWindow(m_device.m_window);
    }
}

void trayser::Engine::Render()
{
    m_renderExtent.width  = m_renderImage.imageExtent.width;
    m_renderExtent.height = m_renderImage.imageExtent.height;

    VkCommandBuffer cmd = m_device.GetCmd();
    BeginRecording(cmd);

    vkutil::TransitionImage(cmd, m_renderImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vkutil::TransitionImage(cmd, m_depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    m_pipelines[PipelineType_Background]->Update();
    m_pipelines[PipelineType_PBR]->Update();

    // Transition the draw image and the swapchain image into their correct transfer layouts
    vkutil::TransitionImage(cmd, m_renderImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::TransitionImage(cmd, m_device.m_swapchain.m_images[m_device.m_swapchain.m_imageIdx], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vkutil::CopyImageToImage(cmd, m_renderImage.image, m_device.m_swapchain.m_images[m_device.m_swapchain.m_imageIdx], m_renderExtent, m_device.m_swapchain.m_extent);
}

void trayser::Engine::Run()
{
    while (!m_device.ShouldQuit())
    {
		m_device.BeginFrame();

        m_scene.Update(1.0f);
        if (!m_editor.m_update)
            m_camera.Input();
        m_editor.Update();

        Render();
        m_device.EndFrame();

        HotReloadPipelines();
    }
}

void trayser::Engine::InitDescriptors()
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

        m_device.m_swapchain.m_frames[i].descriptors = DescriptorAllocatorGrowable{};
        m_device.m_swapchain.m_frames[i].descriptors.Init(m_device.m_device, 1000, frame_sizes);

        m_deletionQueue.Push([&, i]() 
        {
                m_device.m_swapchain.m_frames[i].descriptors.DestroyPools(m_device.m_device);
        });
    }
}

void trayser::Engine::InitPipelines()
{
	m_pipelines.push_back(new trayser::BackgroundPipeline());
	m_pipelines.push_back(new trayser::PBRPipeline());

	for (auto& pipeline : m_pipelines)
	{
		pipeline->Init();
	}
}

void trayser::Engine::InitImGuiStyle()
{
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
    style->Colors[ImGuiCol_Header]                  = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
    style->Colors[ImGuiCol_HeaderHovered]           = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
    style->Colors[ImGuiCol_HeaderActive]            = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
    style->Colors[ImGuiCol_ResizeGrip]              = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style->Colors[ImGuiCol_ResizeGripHovered]       = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
    style->Colors[ImGuiCol_ResizeGripActive]        = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
    style->Colors[ImGuiCol_PlotLines]               = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
    style->Colors[ImGuiCol_PlotLinesHovered]        = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
    style->Colors[ImGuiCol_PlotHistogram]           = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
    style->Colors[ImGuiCol_PlotHistogramHovered]    = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
    style->Colors[ImGuiCol_TextSelectedBg]          = ImVec4(0.45f, 0.45f, 0.00f, 0.43f);
    style->Colors[ImGuiCol_SeparatorHovered]        = ImVec4(0.72f, 0.75f, 0.10f, 0.78f);
    style->Colors[ImGuiCol_SeparatorActive]         = ImVec4(0.70f, 0.75f, 0.10f, 1.00f);
    style->Colors[ImGuiCol_Tab]                     = ImVec4(0.37f, 0.08f, 0.08f, 0.86f);
    style->Colors[ImGuiCol_TabHovered]              = ImVec4(0.87f, 0.00f, 0.00f, 0.80f);
    style->Colors[ImGuiCol_TabActive]               = ImVec4(0.87f, 0.00f, 0.00f, 1.00f);
    style->Colors[ImGuiCol_TabUnfocused]            = ImVec4(0.75f, 0.00f, 0.00f, 0.97f);
    style->Colors[ImGuiCol_TabUnfocusedActive]      = ImVec4(0.61f, 0.00f, 0.00f, 1.00f);
}

void trayser::Engine::InitDefaultData()
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

void trayser::Engine::InitDefaultMaterial()
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

void trayser::Engine::CreateSwapchainImageView()
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
    vmaCreateImage(m_device.m_allocator, &rimg_info, &rimg_allocinfo, &m_renderImage.image, &m_renderImage.allocation, nullptr);

    // Build a image-view for the draw image to use for rendering
    VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(m_renderImage.imageFormat, m_renderImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

    VK_CHECK(vkCreateImageView(m_device.m_device, &rview_info, nullptr, &m_renderImage.imageView));
    
    // Depth image
    m_depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
    m_renderImage.imageExtent = drawImageExtent;

    VkImageUsageFlags depthImageUsages{};
    depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    VkImageCreateInfo dimg_info = vkinit::image_create_info(m_depthImage.imageFormat, depthImageUsages, drawImageExtent);
    vmaCreateImage(m_device.m_allocator, &dimg_info, &rimg_allocinfo, &m_depthImage.image, &m_depthImage.allocation, nullptr);
    VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(m_depthImage.imageFormat, m_depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);
    VK_CHECK(vkCreateImageView(m_device.m_device, &dview_info, nullptr, &m_depthImage.imageView));
}

AllocatedBuffer trayser::Engine::CreateBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
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
    VK_CHECK(vmaCreateBuffer(m_device.m_allocator, &bufferInfo, &vmaallocInfo, &newBuffer.buffer, &newBuffer.allocation,
        &newBuffer.info));

    return newBuffer;
}

AllocatedImage trayser::Engine::CreateImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
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
    VK_CHECK(vmaCreateImage(m_device.m_allocator, &img_info, &allocinfo, &newImage.image, &newImage.allocation, nullptr));

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

AllocatedImage trayser::Engine::CreateImage(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
    size_t data_size = size.depth * size.width * size.height * 4;
    AllocatedBuffer uploadbuffer = CreateBuffer(data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    memcpy(uploadbuffer.info.pMappedData, data, data_size);

    AllocatedImage new_image = CreateImage(size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped);

    VkCommandBuffer cmd;
    m_device.BeginOneTimeSubmit(cmd);

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

    m_device.EndOneTimeSubmit();

    DestroyBuffer(uploadbuffer);

    return new_image;
}

void trayser::Engine::DestroyImage(const AllocatedImage& img)
{
    vkDestroyImageView(m_device.m_device, img.imageView, nullptr);
    vmaDestroyImage(m_device.m_allocator, img.image, img.allocation);
}

gpu::MeshBuffers trayser::Engine::UploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices)
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
    vmaGetAllocationInfo(m_device.m_allocator, staging.allocation, &info);
    void* data = info.pMappedData; // safe to use

    // copy vertex buffer
    memcpy(data, vertices.data(), vertexBufferSize);
    // copy index buffer
    memcpy((char*)data + vertexBufferSize, indices.data(), indexBufferSize);

    VkCommandBuffer cmd;
    m_device.BeginOneTimeSubmit(cmd);

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

    m_device.EndOneTimeSubmit();

    DestroyBuffer(staging);

    return newSurface;
}

void trayser::Engine::DestroyBuffer(const AllocatedBuffer& buffer)
{
    vmaDestroyBuffer(m_device.m_allocator, buffer.buffer, buffer.allocation);
}

void trayser::Engine::HotReloadPipelines()
{
    for (int i = 0; i < m_pipelines.size(); i++)
    {
		m_pipelines[i]->ReloadIfChanged();
    }
}

void trayser::Engine::DestroySwapchain()
{
    vkDestroySwapchainKHR(m_device.m_device, m_device.m_swapchain.m_swapchain, nullptr);

    for (int i = 0; i < m_device.m_swapchain.m_imageViews.size(); i++)
    {
        vkDestroyImageView(m_device.m_device, m_device.m_swapchain.m_imageViews[i], nullptr);
    }
}

void trayser::Engine::BeginRecording(VkCommandBuffer cmd)
{
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));
}

void trayser::Engine::ResizeSwapchain()
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
    vmaDestroyImage(m_device.m_allocator, m_renderImage.image, m_renderImage.allocation);

    CreateSwapchainImageView();

    m_resizeRequested = false;
}