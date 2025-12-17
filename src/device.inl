#include "device.h"
inline VkCommandPoolCreateInfo trayser::CommandPoolCreateInfo(const void* pNext)
{
    return VkCommandPoolCreateInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .pNext = pNext };
}

inline VkCommandBufferAllocateInfo trayser::CommandBufferAllocateInfo(const void* pNext)
{
    return VkCommandBufferAllocateInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .pNext = pNext };
}

inline VkCommandBufferBeginInfo trayser::CommandBufferBeginInfo(const void* pNext)
{
    return VkCommandBufferBeginInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .pNext = pNext };
}

inline VkFenceCreateInfo trayser::FenceCreateInfo(const void* pNext)
{
    return VkFenceCreateInfo{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .pNext = pNext };
}

inline VkSemaphoreCreateInfo trayser::SemaphoreCreateInfo(const void* pNext)
{
    return VkSemaphoreCreateInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = pNext };
}

inline VkSemaphoreSubmitInfo trayser::SemaphoreSubmitInfo(const void* pNext)
{
    return VkSemaphoreSubmitInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO, .pNext = pNext };
}

inline VkCommandBufferSubmitInfo trayser::CommandBufferSubmitInfo(const void* pNext)
{
    return VkCommandBufferSubmitInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO, .pNext = pNext };
}

inline VkSubmitInfo2 trayser::SubmitInfo2(const void* pNext)
{
    return VkSubmitInfo2{ .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2, .pNext = pNext };
}

inline VkPresentInfoKHR trayser::PresentInfoKHR(const void* pNext)
{
    return VkPresentInfoKHR{ .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, .pNext = pNext };
}

inline VkRenderingAttachmentInfo trayser::RenderingAttachmentInfo(const void* pNext)
{
    return VkRenderingAttachmentInfo{ .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, .pNext = pNext };
}

inline VkRenderingInfo trayser::RenderingInfo(const void* pNext)
{
    return VkRenderingInfo{ .sType = VK_STRUCTURE_TYPE_RENDERING_INFO, .pNext = pNext };
}

inline VkWriteDescriptorSet trayser::WriteDescriptorSet(const void* pNext)
{
    return VkWriteDescriptorSet{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = pNext };
}

inline VkImageCreateInfo trayser::ImageCreateInfo(const void* pNext)
{
    return VkImageCreateInfo{ .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .pNext = pNext };
}

inline VkImageViewCreateInfo trayser::ImageViewCreateInfo(const void* pNext)
{
    return VkImageViewCreateInfo{ .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .pNext = pNext };
}

inline VkPipelineLayoutCreateInfo trayser::PipelineLayoutCreateInfo(const void* pNext)
{
    return VkPipelineLayoutCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .pNext = pNext };
}

inline VkPipelineShaderStageCreateInfo trayser::PipelineShaderStageCreateInfo(const void* pNext)
{
    return VkPipelineShaderStageCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .pNext = pNext };
}

VkComputePipelineCreateInfo trayser::ComputePipelineCreateInfo(const void* pNext)
{
    return VkComputePipelineCreateInfo{ .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, .pNext = pNext };
}

VkSamplerCreateInfo trayser::SamplerCreateInfo(const void* pNext)
{
    return VkSamplerCreateInfo{ .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = pNext };
}

VkAccelerationStructureCreateInfoKHR trayser::AccelerationStructureCreateInfoKHR(const void* pNext)
{
    return VkAccelerationStructureCreateInfoKHR{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR, .pNext = pNext };
}

VkBufferCreateInfo trayser::BufferCreateInfo(const void* pNext)
{
    return VkBufferCreateInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .pNext = pNext };
}
