#pragma once

#include <types.h>
#include <unordered_map>
#include <filesystem>
#include <mikktspace.h>

class VulkanEngine;

struct GeoSurface {
    uint32_t startIndex;
    uint32_t count;
};

struct MeshAsset {
    std::string name;

    std::vector<GeoSurface> surfaces;
    gpu::MeshBuffers meshBuffers;
};

struct Primitive
{
    u32 baseVertex;
    u32 vertexCount;
};

struct LoadingMesh
{
    Vertex* vertices;
    u32     vertexCount;
};

struct Mesh
{
    std::vector<Primitive>  primitives;
    AllocatedBuffer         indexBuffer;
    AllocatedBuffer         vertexBuffer;
    VkDeviceAddress         vertexBufferAddr;

    Mesh(VulkanEngine* engine, tinygltf::Model& loaded, const tinygltf::Mesh& loadedMesh, const std::string& folder);
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

public:
    Model(std::string_view path, VulkanEngine* engine);
    void TraverseNode(tinygltf::Model&, const tinygltf::Node&, Node*, const std::string&);
};

//forward declaration
class VulkanEngine;

std::string ReadTextFile(const std::string& path);
std::vector<char> ReadBinaryFile(const std::string& path);

std::optional<std::vector<std::shared_ptr<MeshAsset>>> LoadglTF(VulkanEngine* engine, std::filesystem::path filePath);