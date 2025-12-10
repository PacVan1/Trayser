#pragma once

#include <vulkan/vulkan.h>
#include <pipelines.h>

namespace trayser
{

class Equi2CubemapPipeline final : public Pipeline
{
public:
    struct PushConstantComp
    {
        uint32_t faceSize;
    };

    enum
    {
        BindingPoints_SrcImage,
        BindingPoints_DstCubemap,
    };

public:
    Equi2CubemapPipeline();
    virtual void Load(VkShaderModule module) override;
    virtual void Update() override;
    virtual VkShaderModule Compile() override;

    void SetImage(AllocatedImage& srcImage, AllocatedImage& dstCubemap);

private:
    VkDescriptorSetLayout m_descriptorSetLayout;
    VkDescriptorSet       m_descriptorSet;
    AllocatedImage*       m_srcImage;
    AllocatedImage*       m_dstCubemap;
};

} // namespace trayser
