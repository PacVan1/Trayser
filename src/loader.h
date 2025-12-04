#pragma once

#include <types.h>
#include <unordered_map>
#include <filesystem>
#include <mikktspace.h>

namespace trayser
{

class Engine;

struct GeoSurface {
    uint32_t startIndex;
    uint32_t count;
};

struct MeshAsset {
    std::string name;

    std::vector<GeoSurface> surfaces;
    gpu::MeshBuffers meshBuffers;
};

struct Image
{
    VkImage         image;
    VkImageView     imageView;
    VmaAllocation   allocation;
    VkExtent3D      imageExtent;
    VkFormat        imageFormat;

    Image(const std::string& path, const tinygltf::Model& model, const tinygltf::Image& image, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
    Image(u32* data, u32 width, u32 height, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
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

struct Primitive
{
    u32 baseVertex;
    u32 vertexCount;
    u32 indexCount;
    u32 materialId;
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
    std::vector<Primitive>  primitives;
    std::vector<Material>   materials;
    AllocatedBuffer         indexBuffer;
    AllocatedBuffer         vertexBuffer;
    VkDeviceAddress         vertexBufferAddr;
    VkDeviceAddress         indexBufferAddr;
	u32                     vertexCount;
	u32                     indexCount;

    Mesh(Engine* engine, tinygltf::Model& loaded, const tinygltf::Mesh& loadedMesh, const std::string& folder);
    void LoadMaterial(const tinygltf::Model&, const tinygltf::Material&, const std::string& folder, Material&);
};

struct Model
{
public:
struct Node
{
    std::shared_ptr<Mesh> mesh;
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
    Model(std::string_view path, Engine* engine);
    void TraverseNode(tinygltf::Model&, const tinygltf::Node&, Node*, const std::string&);
};

std::string ReadTextFile(const std::string& path);
std::vector<char> ReadBinaryFile(const std::string& path);

std::optional<std::vector<std::shared_ptr<MeshAsset>>> LoadglTF(Engine* engine, std::filesystem::path filePath);

} // namespace trayser