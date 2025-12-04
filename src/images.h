#pragma once 

#include <vulkan/vulkan.h>

namespace vkutil 
{

void TransitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);
void CopyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize);

} // namespace vkutil

namespace trayser
{

[[nodiscard]] inline u32 GetMaxMipLevels(u32 width, u32 height) { return static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1; }

} // namespace trayser