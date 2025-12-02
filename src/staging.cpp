#include <pch.h>
#include "staging.h"

VkResult trayser::StageBuffer(Buffer& outBuffer, void* data, size_t size)
{
    auto& device = g_engine.m_device;
    VkResult result;

    Buffer staging;
    result = device.CreateBuffer(staging, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	if (result != VK_SUCCESS)
	{
		printf("Failed to create staging buffer\n");
		return result;
	}

    VmaAllocationInfo info;
    vmaGetAllocationInfo(device.m_allocator, staging.allocation, &info);
    memcpy(info.pMappedData, data, size);

    VkCommandBuffer cmd;
    device.BeginOneTimeSubmit(cmd);

    VkBufferCopy bufferCopy{ 0 };
    bufferCopy.dstOffset = 0;
    bufferCopy.srcOffset = 0;
    bufferCopy.size = size;
    vkCmdCopyBuffer(cmd, staging.buffer, outBuffer.buffer, 1, &bufferCopy);

    device.EndOneTimeSubmit();

    //engine->DestroyBuffer(staging);

    return VK_SUCCESS;
}