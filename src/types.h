#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h>

#include <fmt/core.h>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include <entt/entt.hpp>

#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err) {                                                      \
            fmt::println("Detected Vulkan error: {}", string_VkResult(err)); \
            abort();                                                    \
        }                                                               \
    } while (0)

// Type aliases
typedef uint32_t        u32;
typedef uint16_t        u16;
typedef uint8_t         u8;
typedef int32_t         s32;
typedef int16_t         s16;
typedef int8_t          s8;
typedef unsigned int    uint;

using Entity = entt::entity;

struct AllocatedImage 
{
    VkImage         image;
    VkImageView     imageView;
    VmaAllocation   allocation;
    VkExtent3D      imageExtent;
    VkFormat        imageFormat;
};

struct AllocatedBuffer 
{
    VkBuffer            buffer;
    VmaAllocation       allocation;
    VmaAllocationInfo   info;
};

struct Vertex
{
    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    glm::vec4 color;
};

namespace gpu
{
// holds the resources needed for a mesh
struct MeshBuffers {

    AllocatedBuffer indexBuffer;
    AllocatedBuffer vertexBuffer;
    VkDeviceAddress vertexBufferAddress;
};

// push constants for our mesh object draws
struct RenderPushConstants {
    glm::mat4 worldMatrix;
    VkDeviceAddress vertexBuffer;
};
struct SceneData {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewproj;
    glm::vec4 ambientColor;
    glm::vec4 sunlightDirection; // w for sun power
    glm::vec4 sunlightColor;
};
}