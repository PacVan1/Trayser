#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <device.h>
#include <engine.h>

namespace trayser
{

VkResult StageBuffer(Buffer& outBuffer, void* data, size_t size);

} // namespace trayser