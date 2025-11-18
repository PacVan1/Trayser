#pragma once

#include <vk_types.h>

class DescriptorLayoutBuilder 
{
public:
    void AddBinding(uint32_t binding, VkDescriptorType type);
    void Clear();
    VkDescriptorSetLayout Build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);

public:
    std::vector<VkDescriptorSetLayoutBinding> m_bindings;
};

class DescriptorAllocator 
{
public:
    struct PoolSizeRatio 
    {
        VkDescriptorType type;
        float ratio;
    };

public:
    void InitPool(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios);
    void ClearDescriptors(VkDevice device);
    void DestroyPool(VkDevice device);
    VkDescriptorSet Allocate(VkDevice device, VkDescriptorSetLayout layout);

public:
    VkDescriptorPool m_pool;

};
