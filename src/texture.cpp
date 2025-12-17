#include <pch.h>

#include <images.h>
#include <util.h>
#include <texture.h>
#include <engine.h>
#include <stb_image.h>

trayser::Texture::~Texture()
{
    g_engine.m_device.DestroyImage(image);
}

trayser::Texture::Texture(const std::string& path, const tinygltf::Model& model, const tinygltf::Image& inImage, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
    auto& engine = g_engine;

    int width, height, channels;
    uint8_t* data = nullptr;

    if (inImage.bufferView > -1)
    {   // Is embedded
        const tinygltf::BufferView& view = model.bufferViews[inImage.bufferView];
        const tinygltf::Buffer& buffer = model.buffers[view.buffer];
        const unsigned char* file = buffer.data.data() + view.byteOffset;
        data = stbi_load_from_memory(file, view.byteLength, &width, &height, &channels, 4);
    }
    else
    {   // Is external, use the path
        data = stbi_load(path.c_str(), &width, &height, &channels, 4);
    }

    if (!data)
    {
        printf("Warning: Failed to load image\n");
        return;
    }

    size_t data_size = width * height * 4;

    Device::StageBuffer stageBuffer;
    VK_CHECK(engine.m_device.CreateStageBuffer(data_size, stageBuffer));

    memcpy(stageBuffer.mapped, data, data_size);

    VkExtent3D extent = { width, height, 1 };

    Device::Image newImage{};
    VkImageCreateInfo imageCreateInfo = ImageCreateInfo();
    imageCreateInfo.format = format;
    imageCreateInfo.usage = usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageCreateInfo.extent = extent;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.mipLevels = GetMaxMipLevels(extent.width, extent.height);
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;

    // always allocate images on dedicated GPU memory
    VmaAllocationCreateInfo allocinfo = {};
    allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // allocate and create the image
    VK_CHECK(vmaCreateImage(g_engine.m_device.m_allocator, &imageCreateInfo, &allocinfo, &newImage.image, &newImage.allocation, nullptr));

    VkCommandBuffer cmd;
    engine.m_device.BeginOneTimeSubmit(cmd);

    vkutil::TransitionImage(cmd, newImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 1);

    VkBufferImageCopy copyRegion = {};
    copyRegion.bufferOffset = 0;
    copyRegion.bufferRowLength = 0;
    copyRegion.bufferImageHeight = 0;

    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent = extent;

    // copy the buffer into the image
    vkCmdCopyBufferToImage(cmd, stageBuffer.buffer, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
        &copyRegion);

    vkutil::TransitionImage(cmd, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 1);

    engine.m_device.EndOneTimeSubmit();

    engine.m_device.DestroyStageBuffer(stageBuffer);
    stbi_image_free(data);

    g_engine.m_device.BeginOneTimeSubmit(cmd);

    // Transition image to layout in which we can perform mip map generation
    vkutil::TransitionImage(cmd, newImage.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 1);

    for (uint32_t mip = 1; mip < imageCreateInfo.mipLevels; ++mip)
    {
        vkutil::TransitionImage(cmd, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, mip - 1, 1);
        vkutil::TransitionImage(cmd, newImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mip, 1);

        VkImageBlit blit = {};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = mip - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.srcOffsets[1] = { static_cast<int32_t>(extent.width >> (mip - 1)), static_cast<int32_t>(extent.height >> (mip - 1)), 1 }; // src extent

        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = mip;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;
        blit.dstOffsets[1] = { static_cast<int32_t>(extent.width >> mip), static_cast<int32_t>(extent.height >> mip), 1 }; // dst extent

        vkCmdBlitImage(
            cmd,
            newImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit, VK_FILTER_LINEAR
        );
    }

    vkutil::TransitionImage(cmd, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, imageCreateInfo.mipLevels - 1);

    // Transition back to usable format
    vkutil::TransitionImage(cmd, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, imageCreateInfo.mipLevels - 1, 1);

    g_engine.m_device.EndOneTimeSubmit();

    // build a image-view for the image
    VkImageViewCreateInfo viewCreateInfo = ImageViewCreateInfo();
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format = format;
    viewCreateInfo.image = newImage.image;
    viewCreateInfo.subresourceRange.baseMipLevel = 0;
    viewCreateInfo.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    viewCreateInfo.subresourceRange.baseArrayLayer = 0;
    viewCreateInfo.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
    viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    VK_CHECK(vkCreateImageView(g_engine.m_device.m_device, &viewCreateInfo, nullptr, &imageView));

    image.image = newImage.image;
    image.allocation = newImage.allocation;
}

trayser::Texture::Texture(u32* data, u32 width, u32 height, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
    auto& engine = g_engine;

    size_t data_size = width * height * 4;
    Device::StageBuffer stageBuffer;
    VK_CHECK(engine.m_device.CreateStageBuffer(data_size, stageBuffer));

    memcpy(stageBuffer.mapped, data, data_size);

    VkExtent3D extent = { width, height, 1 };

    VkImageCreateInfo createInfo = ImageCreateInfo();
    createInfo.format = format;
    createInfo.usage = usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    createInfo.extent = extent;
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.mipLevels = 1;
    createInfo.arrayLayers = 1;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.flags = 0;
    allocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    Device::Image new_image;
    engine.m_device.CreateImage(createInfo, allocCreateInfo, new_image);

    VkImageViewCreateInfo viewCreateInfo = ImageViewCreateInfo();
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format = format;
    viewCreateInfo.image = new_image.image;
    viewCreateInfo.subresourceRange.baseMipLevel = 0;
    viewCreateInfo.subresourceRange.levelCount = 1;
    viewCreateInfo.subresourceRange.baseArrayLayer = 0;
    viewCreateInfo.subresourceRange.layerCount = 1;
    viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewCreateInfo.subresourceRange.levelCount = createInfo.mipLevels;
    VK_CHECK(vkCreateImageView(g_engine.m_device.m_device, &viewCreateInfo, nullptr, &imageView));

    VkCommandBuffer cmd;
    engine.m_device.BeginOneTimeSubmit(cmd);

    vkutil::TransitionImage(cmd, new_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkBufferImageCopy copyRegion = {};
    copyRegion.bufferOffset = 0;
    copyRegion.bufferRowLength = 0;
    copyRegion.bufferImageHeight = 0;

    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent = extent;

    // copy the buffer into the image
    vkCmdCopyBufferToImage(cmd, stageBuffer.buffer, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
        &copyRegion);

    vkutil::TransitionImage(cmd, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    engine.m_device.EndOneTimeSubmit();

    engine.m_device.DestroyStageBuffer(stageBuffer);

    image.image = new_image.image;
    image.allocation = new_image.allocation;
}

trayser::Texture::Texture(u16* data, u32 width, u32 height, VkFormat format, VkImageUsageFlags usage, HDRI)
{
    auto& engine = g_engine;

    size_t data_size = width * height * 4 * 2;
    Device::StageBuffer stageBuffer;
    VK_CHECK(engine.m_device.CreateStageBuffer(data_size, stageBuffer));

    memcpy(stageBuffer.mapped, data, data_size);

    VkExtent3D extent = { width, height, 1 };

    //AllocatedImage new_image = engine.CreateImage(extent, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, false);

    VkImageCreateInfo createInfo = ImageCreateInfo();
    createInfo.format = format;
    createInfo.usage = usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    createInfo.extent = extent;
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.mipLevels = 1;
    createInfo.arrayLayers = 1;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.flags = 0;
    allocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    Device::Image new_image;
    engine.m_device.CreateImage(createInfo, allocCreateInfo, new_image);

    VkImageViewCreateInfo viewCreateInfo = ImageViewCreateInfo();
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format = format;
    viewCreateInfo.image = new_image.image;
    viewCreateInfo.subresourceRange.baseMipLevel = 0;
    viewCreateInfo.subresourceRange.levelCount = 1;
    viewCreateInfo.subresourceRange.baseArrayLayer = 0;
    viewCreateInfo.subresourceRange.layerCount = 1;
    viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewCreateInfo.subresourceRange.levelCount = createInfo.mipLevels;
    VK_CHECK(vkCreateImageView(g_engine.m_device.m_device, &viewCreateInfo, nullptr, &imageView));

    VkCommandBuffer cmd;
    engine.m_device.BeginOneTimeSubmit(cmd);

    vkutil::TransitionImage(cmd, new_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkBufferImageCopy copyRegion = {};
    copyRegion.bufferOffset = 0;
    copyRegion.bufferRowLength = 0;
    copyRegion.bufferImageHeight = 0;

    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent = extent;

    // copy the buffer into the image
    vkCmdCopyBufferToImage(cmd, stageBuffer.buffer, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
        &copyRegion);

    vkutil::TransitionImage(cmd, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    engine.m_device.EndOneTimeSubmit();

    engine.m_device.DestroyStageBuffer(stageBuffer);

    image.image = new_image.image;
    image.allocation = new_image.allocation;
}

trayser::Texture::Texture(f16* data, u32 width, u32 height, VkImageUsageFlags usage, HDRI)
{
    VkDeviceSize sizeBytes = VkDeviceSize{ width * height * 4 * 2 }; // w * h * channels * bytes/pixel

    Device::StageBuffer stageBuffer;
    VK_CHECK(g_engine.m_device.CreateStageBuffer(sizeBytes, stageBuffer));

    memcpy(stageBuffer.mapped, data, sizeBytes);

    VkExtent3D extent = { width, height, 1 };

    //AllocatedImage new_image = g_engine.CreateImage(extent, VK_FORMAT_R16G16B16A16_SFLOAT, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, false);

    VkImageCreateInfo createInfo = ImageCreateInfo();
    createInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    createInfo.usage = usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    createInfo.extent = extent;
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.mipLevels = 1;
    createInfo.arrayLayers = 1;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.flags = 0;
    allocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    Device::Image new_image;
    g_engine.m_device.CreateImage(createInfo, allocCreateInfo, new_image);

    VkImageViewCreateInfo viewCreateInfo = ImageViewCreateInfo();
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    viewCreateInfo.image = new_image.image;
    viewCreateInfo.subresourceRange.baseMipLevel = 0;
    viewCreateInfo.subresourceRange.levelCount = 1;
    viewCreateInfo.subresourceRange.baseArrayLayer = 0;
    viewCreateInfo.subresourceRange.layerCount = 1;
    viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewCreateInfo.subresourceRange.levelCount = createInfo.mipLevels;
    VK_CHECK(vkCreateImageView(g_engine.m_device.m_device, &viewCreateInfo, nullptr, &imageView));

    VkCommandBuffer cmd;
    g_engine.m_device.BeginOneTimeSubmit(cmd);

    vkutil::TransitionImage(cmd, new_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkBufferImageCopy copyRegion = {};
    copyRegion.bufferOffset = 0;
    copyRegion.bufferRowLength = 0;
    copyRegion.bufferImageHeight = 0;

    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent = extent;

    // copy the buffer into the image
    vkCmdCopyBufferToImage(cmd, stageBuffer.buffer, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
        &copyRegion);

    vkutil::TransitionImage(cmd, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    g_engine.m_device.EndOneTimeSubmit();
    g_engine.m_device.DestroyStageBuffer(stageBuffer);

    image.image = new_image.image;
    image.allocation = new_image.allocation;
}