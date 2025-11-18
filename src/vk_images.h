#pragma once 

#include <vulkan/vulkan.h>

namespace vkutil 
{

void TransitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);

} // namespace vkutil