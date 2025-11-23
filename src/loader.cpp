#include <pch.h>
#include <loader.h>

#include "stb_image.h"
#include <iostream>

#include "engine.h"
#include "initializers.h"
#include "types.h"
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>

std::optional<std::vector<std::shared_ptr<MeshAsset>>> LoadglTF(VulkanEngine* engine, std::filesystem::path filePath)
{
    std::cout << "Loading GLTF: " << filePath << std::endl;

    fastgltf::GltfDataBuffer data;
    data.loadFromFile(filePath);

    constexpr auto gltfOptions = fastgltf::Options::LoadGLBBuffers
        | fastgltf::Options::LoadExternalBuffers;

    fastgltf::Asset gltf;
    fastgltf::Parser parser{};

    auto load = parser.loadBinaryGLTF(&data, filePath.parent_path(), gltfOptions);
    if (load) {
        gltf = std::move(load.get());
    }
    else {
        fmt::print("Failed to load glTF: {} \n", fastgltf::to_underlying(load.error()));
        return {};
    }

    std::vector<std::shared_ptr<MeshAsset>> meshes;

    // use the same vectors for all meshes so that the memory doesnt reallocate as
    // often
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;
    for (fastgltf::Mesh& mesh : gltf.meshes) {
        MeshAsset newmesh;

        newmesh.name = mesh.name;

        // clear the mesh arrays each mesh, we dont want to merge them by error
        indices.clear();
        vertices.clear();

        for (auto&& p : mesh.primitives) {
            GeoSurface newSurface;
            newSurface.startIndex = (uint32_t)indices.size();
            newSurface.count = (uint32_t)gltf.accessors[p.indicesAccessor.value()].count;

            size_t initial_vtx = vertices.size();

            // load indexes
            {
                fastgltf::Accessor& indexaccessor = gltf.accessors[p.indicesAccessor.value()];
                indices.reserve(indices.size() + indexaccessor.count);

                fastgltf::iterateAccessor<std::uint32_t>(gltf, indexaccessor,
                    [&](std::uint32_t idx) {
                        indices.push_back(idx + initial_vtx);
                    });
            }

            // load vertex positions
            {
                fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute("POSITION")->second];
                vertices.resize(vertices.size() + posAccessor.count);

                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
                    [&](glm::vec3 v, size_t index) {
                        Vertex newvtx;
                        newvtx.position = v;
                        newvtx.normal = { 1, 0, 0 };
                        newvtx.color = glm::vec4{ 1.f };
                        newvtx.uv_x = 0;
                        newvtx.uv_y = 0;
                        vertices[initial_vtx + index] = newvtx;
                    });
            }

            // load vertex normals
            auto normals = p.findAttribute("NORMAL");
            if (normals != p.attributes.end()) {

                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[(*normals).second],
                    [&](glm::vec3 v, size_t index) {
                        vertices[initial_vtx + index].normal = v;
                    });
            }

            // load UVs
            auto uv = p.findAttribute("TEXCOORD_0");
            if (uv != p.attributes.end()) {

                fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[(*uv).second],
                    [&](glm::vec2 v, size_t index) {
                        vertices[initial_vtx + index].uv_x = v.x;
                        vertices[initial_vtx + index].uv_y = v.y;
                    });
            }

            // load vertex colors
            auto colors = p.findAttribute("COLOR_0");
            if (colors != p.attributes.end()) {

                fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[(*colors).second],
                    [&](glm::vec4 v, size_t index) {
                        vertices[initial_vtx + index].color = v;
                    });
            }
            newmesh.surfaces.push_back(newSurface);
        }

        // display the vertex normals
        constexpr bool OverrideColors = true;
        if (OverrideColors) {
            for (Vertex& vtx : vertices) {
                vtx.color = glm::vec4(vtx.normal, 1.f);
            }
        }
        newmesh.meshBuffers = engine->UploadMesh(indices, vertices);

        meshes.emplace_back(std::make_shared<MeshAsset>(std::move(newmesh)));
    }

    return meshes;
}

std::shared_ptr<Model> LoadModel(VulkanEngine* engine, std::string_view filePath)
{
    return std::shared_ptr<Model>();
}

std::shared_ptr<Mesh> LoadMesh(const std::string& name, tinygltf::Model& loaded, const tinygltf::Mesh& loadedMesh, const std::string& folder)
{
    return std::shared_ptr<Mesh>();
}

void Model::TraverseNode(tinygltf::Model& loaded, const tinygltf::Node& loadedNode, Node* parent, const std::string& folder)
{
    auto& node = nodes.emplace_back();
    if (loadedNode.matrix.empty())
    {
        // If there is no local matrix, use any combination of 
        // translation, rotation and scale to compute the local matrix
        node.position = loadedNode.translation.empty() ? glm::vec3(0.0f) : glm::vec3(glm::make_vec3(loadedNode.translation.data()));
        node.scale = loadedNode.scale.empty() ? glm::vec3(1.0f) : glm::vec3(glm::make_vec3(loadedNode.scale.data()));

        if (loadedNode.rotation.empty())
            node.rotation = glm::quat();
        else
            node.rotation = glm::quat(loadedNode.rotation[3], loadedNode.rotation[0], loadedNode.rotation[1], loadedNode.rotation[2]);

        glm::mat4 T = glm::translate(glm::mat4(1.0f), node.position);
        glm::mat4 R = glm::toMat4(node.rotation);
        glm::mat4 S = glm::scale(glm::mat4(1.0f), node.scale);
        node.matrix = T * R * S;
    }
    else
    {
        // If there is a local matrix, use it
        node.matrix = glm::make_mat4(loadedNode.matrix.data());

        glm::vec3 skew;
        glm::vec4 perspective;
        glm::decompose(node.matrix, node.scale, node.rotation, node.position, skew, perspective);
    }

    if (parent)
    {
        // If the node has a parent,
        // then register it as one of the parent's childs
        parent->children.push_back(nodes.size() - 1);
    }
    else
    {
        // If the node has no parent (so is a root node), 
        // then register as one of the root nodes
        rootNodes.push_back(nodes.size() - 1);
    }

    if (loadedNode.mesh >= 0)
    {   // Does the node have a mesh
        std::string name = folder + "_mesh" + std::to_string(loadedNode.mesh);

        auto& loadedMesh = loaded.meshes[loadedNode.mesh];
        //node.mesh = Engine.Resources().Load<IndirectMesh>(FileIO::Directory::None, name, loaded, loadedMesh, folder);
    }

    for (int nodeIdx : loadedNode.children)
    {
        TraverseNode(loaded, loaded.nodes[nodeIdx], &node, folder);
    }
}
