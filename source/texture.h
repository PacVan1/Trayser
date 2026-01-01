#pragma once

#include <device.h>
#include <vulkan/vulkan.h>
#include <tiny_gltf.h>

namespace trayser
{

struct Texture
{
    // Tagging
    struct HDRI {};

    Device::Image   image;
    VkImageView     imageView;

    Texture() = default;
    ~Texture();
    Texture(const std::string& path, const tinygltf::Model& model, const tinygltf::Image& image, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
    Texture(u32* data, u32 width, u32 height, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
    Texture(u16* data, u32 width, u32 height, VkFormat format, VkImageUsageFlags usage, HDRI);
    Texture(f16* data, u32 width, u32 height, VkImageUsageFlags usage, HDRI);
};

} // namespace trayser