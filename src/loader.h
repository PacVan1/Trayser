#pragma once

#include <types.h>
#include <unordered_map>
#include <filesystem>

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

struct Mesh
{
    std::vector<Primitive>  primitives;
    AllocatedBuffer         indexBuffer;
    AllocatedBuffer         vertexBuffer;
    VkDeviceAddress         vertexBufferAddr;
};

struct Model
{
struct Node
{
    std::shared_ptr<Mesh> mesh;
    std::vector<int> children;
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation;
    glm::vec3 scale = glm::vec3(1.0f);
    glm::mat4 matrix = glm::mat4(1.0f);
};

public:
    std::vector<Node>   nodes;
    std::vector<int>    rootNodes;

private:
    void TraverseNode(tinygltf::Model&, const tinygltf::Node&, Node*, const std::string&);

};

//forward declaration
class VulkanEngine;

std::optional<std::vector<std::shared_ptr<MeshAsset>>> LoadglTF(VulkanEngine* engine, std::filesystem::path filePath);
std::shared_ptr<Model>  LoadModel(VulkanEngine* engine, std::string_view filePath);
std::shared_ptr<Mesh>   LoadMesh(const std::string& name, tinygltf::Model& loaded, const tinygltf::Mesh& loadedMesh, const std::string& folder);