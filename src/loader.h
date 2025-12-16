#pragma once

#include <types.h>
#include <unordered_map>
#include <filesystem>
#include <mikktspace.h>
#include <resources.h>
#include <device.h>

namespace trayser
{

using ModelHandle = ResourceHandle;
using MeshHandle = ResourceHandle;
using TextureHandle = ResourceHandle;
using MaterialHandle = ResourceHandle;

class Engine;

struct Image
{
    // Tagging
    struct HDRI {};

    VkImage         image;
    VkImageView     imageView;
    VmaAllocation   allocation;
    VkExtent3D      imageExtent;
    VkFormat        imageFormat;

    Image() = default;
    ~Image();
    Image(const std::string& path, const tinygltf::Model& model, const tinygltf::Image& image, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
    Image(const std::string& path, VkFormat format, VkImageUsageFlags usage);
    Image(u32* data, u32 width, u32 height, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
    Image(u16* data, u32 width, u32 height, VkFormat format, VkImageUsageFlags usage, HDRI);
    Image(f16* data, u32 width, u32 height, VkImageUsageFlags usage, HDRI);
};

struct Material
{
    std::shared_ptr<Image> baseColor;
    std::shared_ptr<Image> normalMap;
    std::shared_ptr<Image> metallicRoughness;
    std::shared_ptr<Image> occlusion;
    std::shared_ptr<Image> emissive;
    glm::vec4 baseColorFactor = glm::vec4(1.0);
    glm::vec3 metallicRoughnessAoFactor = glm::vec3(1.0);
    glm::vec3 emissiveFactor = glm::vec3(1.0);
};

struct Material2
{
    // Tagging
    struct Default {};

    ResourceHandle baseColorHandle    = ResourceHandle_Invalid;   // Base color index to textures
    ResourceHandle normalMapHandle    = ResourceHandle_Invalid;   // Normal map index to textures
    ResourceHandle metalRoughHandle   = ResourceHandle_Invalid;   // Metallic roughness index to textures
    ResourceHandle aoHandle           = ResourceHandle_Invalid;   // Ambient occlusion index to textures
    ResourceHandle emissiveHandle     = ResourceHandle_Invalid;   // Emissive index to textures

    float4 baseColorFactor              = float4(1.0);
    float4 metallicRoughnessAoFactor    = float4(0.5, 0.5, 1.0, 1.0);
    float4 emissiveFactor               = float4(0.0);

    Material2() = default;
    Material2(Default);
    Material2(const tinygltf::Model&, const tinygltf::Material&, const std::string& folder);
};

struct Primitive
{
    u32 baseVertex;
    u32 vertexCount;
    u32 baseIndex;
    u32 indexCount;
    MaterialHandle materialId;
};

struct LoadingMesh
{
    Vertex* vertices;
    u32*    indices;
    u32     vertexCount;
    u32     indexCount;
};

struct Instance
{
    glm::mat4 transform;
    u32 meshIdx;
};

struct Mesh
{
    Device::AccelerationStructure   BLas;
    std::vector<Primitive>  primitives;
    AllocatedBuffer         indexBuffer;
    AllocatedBuffer         vertexBuffer;
    AllocatedBuffer         primitiveBuffer;
    VkDeviceAddress         vertexBufferAddr;
    VkDeviceAddress         indexBufferAddr;
    VkDeviceAddress         primitiveBufferAddr;
	u32                     vertexCount;
	u32                     indexCount;

    Mesh() = default;
    ~Mesh();
    Mesh(Engine* engine, tinygltf::Model& loaded, const tinygltf::Mesh& loadedMesh, const std::string& folder);
    MaterialHandle LoadMaterial(const tinygltf::Model&, int matIdx, const std::string& folder);
    void PrimitivesToGeometries(std::vector<VkAccelerationStructureGeometryKHR>& geometries,
        std::vector<VkAccelerationStructureBuildRangeInfoKHR>& rangeInfos);
    void InitBLas();
};

struct Model
{
public:
struct Node
{
    //std::shared_ptr<Mesh> mesh;
    MeshHandle handle;
    std::vector<int> children;
    glm::vec3 translation = glm::vec3(0.0f);
    glm::quat orientation;
    glm::vec3 scale = glm::vec3(1.0f);
    glm::mat4 matrix = glm::mat4(1.0f);
};

public:
    inline static SMikkTSpaceInterface g_mikkTSpaceIface;
    inline static SMikkTSpaceContext   g_mikkTSpaceCtx;

public:
    std::vector<Node>   nodes;
    std::vector<int>    rootNodes;

public:
    static void MikkTSpaceInit();
    static void MikkTSpaceCalc(LoadingMesh* mesh);
    static int  MikkTSpaceGetNumFaces(const SMikkTSpaceContext* context);
    static int  MikkTSpaceGetNumVerticesOfFace(const SMikkTSpaceContext* context, const int iFace);
    static void MikkTSpaceGetPosition(const SMikkTSpaceContext* context, float* outPos, const int iFace, const int iVert);
    static void MikkTSpaceGetNormal(const SMikkTSpaceContext* context, float* outNormal, const int iFace, const int iVert);
    static void MikkTSpaceGetTexCoords(const SMikkTSpaceContext* context, float* outUv, const int iFace, const int iVert);
    static void MikkTSpaceSetTSpaceBasic(const SMikkTSpaceContext* context,
        const float* tangentu,
        const float fSign,
        const int iFace,
        const int iVert);
    static int  MikkTSpaceGetVertexIndex(const SMikkTSpaceContext* context, int iFace, int iVert);
    static int TotalIndexCountInMesh(const tinygltf::Model& loaded, const tinygltf::Mesh& loadedMesh)
    {
        int indexCount = 0;

        for (const auto& prim : loadedMesh.primitives)
        {
            if (prim.indices >= 0)
            {   // Has indices
                const tinygltf::Accessor& accessor = loaded.accessors[prim.indices];
                indexCount += accessor.count;
            }
        }

        return indexCount;
    }
    static int TotalVertexCountInMesh(const tinygltf::Model& loaded, const tinygltf::Mesh& loadedMesh)
    {
        int vertexCount = 0;

        for (const auto& prim : loadedMesh.primitives)
        {
            auto it = prim.attributes.find("POSITION");
            if (it != prim.attributes.end())
            {   // Has vertices
                int accessorIdx = it->second;
                const tinygltf::Accessor& accessor = loaded.accessors[accessorIdx];
                vertexCount += accessor.count;
            }
        }

        return vertexCount;
    }

public:
    Model() = default;
    Model(std::string_view path, Engine* engine);
    void TraverseNode(tinygltf::Model&, const tinygltf::Node&, Node*, const std::string&);
};

} // namespace trayser