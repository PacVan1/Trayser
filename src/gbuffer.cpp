#include <pch.h>

#include <gbuffer.h>
#include <engine.h>
#include <device.h>

void trayser::GBuffer::Init(u32 width, u32 height)
{
    // Draw image size will match the window
    VkExtent3D imageExtent =
    {
        width,
        height,
        1
    };

    // Hardcoding the render format to 32 bit float
    colorImage.imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
    colorImage.imageExtent = imageExtent;

    VmaAllocationCreateInfo imageAllocInfo = {};
    imageAllocInfo.usage         = VMA_MEMORY_USAGE_GPU_ONLY;
    imageAllocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    { // Color image
        VkImageUsageFlags imageUsages{};
        imageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
        imageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        VkImageCreateInfo imageCreateInfo = ImageCreateInfo();
        imageCreateInfo.format      = colorImage.imageFormat;
        imageCreateInfo.usage       = imageUsages;
        imageCreateInfo.extent      = imageExtent;
        imageCreateInfo.imageType   = VK_IMAGE_TYPE_2D;
        imageCreateInfo.mipLevels   = 1;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.samples     = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling      = VK_IMAGE_TILING_OPTIMAL;
        VK_CHECK(vmaCreateImage(g_engine.m_device.m_allocator, &imageCreateInfo, &imageAllocInfo, &colorImage.image, &colorImage.allocation, nullptr));

        VkImageViewCreateInfo viewCreateInfo = ImageViewCreateInfo();
        viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewCreateInfo.format = colorImage.imageFormat;
        viewCreateInfo.image = colorImage.image;
        viewCreateInfo.subresourceRange.baseMipLevel = 0;
        viewCreateInfo.subresourceRange.levelCount = 1;
        viewCreateInfo.subresourceRange.baseArrayLayer = 0;
        viewCreateInfo.subresourceRange.layerCount = 1;
        viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        VK_CHECK(vkCreateImageView(g_engine.m_device.m_device, &viewCreateInfo, nullptr, &colorImage.imageView));
    }

    // Depth image
    depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
    depthImage.imageExtent = imageExtent;

    { // Depth image
        VkImageUsageFlags imageUsages{};
        imageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        VkImageCreateInfo imageCreateInfo = ImageCreateInfo();
        imageCreateInfo.format      = depthImage.imageFormat;
        imageCreateInfo.usage       = imageUsages;
        imageCreateInfo.extent      = imageExtent;
        imageCreateInfo.imageType   = VK_IMAGE_TYPE_2D;
        imageCreateInfo.mipLevels   = 1;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.samples     = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling      = VK_IMAGE_TILING_OPTIMAL;
        VK_CHECK(vmaCreateImage(g_engine.m_device.m_allocator, &imageCreateInfo, &imageAllocInfo, &depthImage.image, &depthImage.allocation, nullptr));

        VkImageViewCreateInfo viewCreateInfo = ImageViewCreateInfo();
        viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewCreateInfo.format = depthImage.imageFormat;
        viewCreateInfo.image = depthImage.image;
        viewCreateInfo.subresourceRange.baseMipLevel = 0;
        viewCreateInfo.subresourceRange.levelCount = 1;
        viewCreateInfo.subresourceRange.baseArrayLayer = 0;
        viewCreateInfo.subresourceRange.layerCount = 1;
        viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        VK_CHECK(vkCreateImageView(g_engine.m_device.m_device, &viewCreateInfo, nullptr, &depthImage.imageView));
    }
}
