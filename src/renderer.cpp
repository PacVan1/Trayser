#include <pch.h>

#include <engine.h>
#include <renderer.h>
#include <gpu_io.h>

void trayser::Renderer::Init(Device& device)
{
    InitCmdPool(device);
	InitTextureDescLayout(device);
	InitFrames(device);
}

void trayser::Renderer::Destroy(Device& device)
{
    DestroyCmdPool(device);
}

void trayser::Renderer::Render(uint32_t frameIndex)
{
}

void trayser::Renderer::InitCmdPool(Device& device)
{
    VkCommandPoolCreateInfo createInfo = CommandPoolCreateInfo();
    createInfo.queueFamilyIndex = device.m_graphicsQueueFamily;
    createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VK_CHECK(vkCreateCommandPool(device.m_device, &createInfo, nullptr, &m_cmdPool));
}

void trayser::Renderer::InitTextureDescLayout(Device& device)
{
    DescriptorSetLayoutBinding binding{};
    binding.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = kTextureCount;
    binding.stageFlags =
        VK_SHADER_STAGE_FRAGMENT_BIT |
        VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
        VK_SHADER_STAGE_MISS_BIT_KHR |
        VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    binding.pImmutableSamplers = nullptr;

    DescriptorSetLayoutBuilder builder{};
    builder.AddBinding(binding);
    builder.Build(device, m_textureDescLayout);
}

void trayser::Renderer::InitFrames(Device& device)
{
    InitCmdBuffers(device);
    InitTextureDescSets(device);
}

void trayser::Renderer::InitCmdBuffers(Device& device)
{
    VkCommandBufferAllocateInfo allocInfo = CommandBufferAllocateInfo();
    allocInfo.commandPool        = m_cmdPool;
    allocInfo.commandBufferCount = kFrameCount;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    VkCommandBuffer cmdBuffers[kFrameCount] = {};
    VK_CHECK(vkAllocateCommandBuffers(device.m_device, &allocInfo, cmdBuffers));

    for (int i = 0; i < kFrameCount; i++)
    {
        m_frames[i].cmdBuffer = cmdBuffers[i];
    }
}

void trayser::Renderer::InitTextureDescSets(Device& device)
{
    for (int i = 0; i < kFrameCount; i++)
    {
        m_frames[i].textureDescSet = g_engine.m_globalDescriptorAllocator.Allocate(device.m_device, m_textureDescLayout);
    }
}

void trayser::Renderer::DestroyCmdPool(Device& device) const
{
    vkDestroyCommandPool(device.m_device, m_cmdPool, nullptr);
}
