#include <pch.h>
#include <loader.h>

#include "stb_image.h"
#include <iostream>
#include <assert.h>
#include <fstream>

#include "engine.h"
#include "initializers.h"
#include "types.h"
#include <images.h>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>

#include <vk_mem_alloc.h>

std::string ReadTextFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        printf("Path doesn't exist");
        return std::string();
    }
    file.seekg(0, std::ios::end);
    const size_t size = file.tellg();
    std::string buffer(size, '\0');
    file.seekg(0);
    file.read(buffer.data(), size);
    return buffer;
}

std::vector<char> ReadBinaryFile(const std::string& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        printf("Path doesn't exist");
        return  std::vector<char>();
    }
    const std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> buffer(size);
    if (file.read(buffer.data(), size)) return buffer;
    assert(false);
    return std::vector<char>();
}

std::optional<std::vector<std::shared_ptr<trayser::MeshAsset>>> trayser::LoadglTF(Engine* engine, std::filesystem::path filePath)
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
                        newvtx.uvX = 0;
                        newvtx.uvY = 0;
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
                        vertices[initial_vtx + index].uvX = v.x;
                        vertices[initial_vtx + index].uvY = v.y;
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

void trayser::Model::TraverseNode(tinygltf::Model& loaded, const tinygltf::Node& loadedNode, Node* parent, const std::string& folder)
{
    auto& node = nodes.emplace_back();
    if (loadedNode.matrix.empty())
    {
        // If there is no local matrix, use any combination of 
        // translation, rotation and scale to compute the local matrix
        node.translation = loadedNode.translation.empty() ? glm::vec3(0.0f) : glm::vec3(glm::make_vec3(loadedNode.translation.data()));
        node.scale = loadedNode.scale.empty() ? glm::vec3(1.0f) : glm::vec3(glm::make_vec3(loadedNode.scale.data()));

        if (loadedNode.rotation.empty())
            node.orientation = glm::quat();
        else
            node.orientation = glm::quat(loadedNode.rotation[3], loadedNode.rotation[0], loadedNode.rotation[1], loadedNode.rotation[2]);

        glm::mat4 T = glm::translate(glm::mat4(1.0f), node.translation);
        glm::mat4 R = glm::toMat4(node.orientation);
        glm::mat4 S = glm::scale(glm::mat4(1.0f), node.scale);
        node.matrix = T * R * S;
    }
    else
    {
        // If there is a local matrix, use it
        node.matrix = glm::make_mat4(loadedNode.matrix.data());

        glm::vec3 skew;
        glm::vec4 perspective;
        glm::decompose(node.matrix, node.scale, node.orientation, node.translation, skew, perspective);
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
        auto& engine = g_engine;
        node.mesh = engine.m_resources.Create<Mesh>(name, &engine, loaded, loadedMesh, folder);
    }

    for (int nodeIdx : loadedNode.children)
    {
        TraverseNode(loaded, loaded.nodes[nodeIdx], &node, folder);
    }
}

trayser::Mesh::Mesh(Engine* engine, tinygltf::Model& loaded, const tinygltf::Mesh& loadedMesh, const std::string& folder)
{
    // Indexing is already being processed during loading,
    // so the amount of indices equals the amount of vertices
    vertexCount = Model::TotalIndexCountInMesh(loaded, loadedMesh);
    if (vertexCount == 0)
    {
        printf("Model loader: mesh misses indices");
        return;
    }

    // NEW /////////////////////////////////////////////////////////
    const size_t vertexBufferSize = vertexCount * sizeof(Vertex);
    const size_t indexBufferSize = 16 * sizeof(u32);

    //create vertex buffer
    vertexBuffer = engine->CreateBuffer(vertexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    //find the adress of the vertex buffer
    VkBufferDeviceAddressInfo deviceAdressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,.buffer = vertexBuffer.buffer };
    vertexBufferAddr = vkGetBufferDeviceAddress(engine->m_device.m_device, &deviceAdressInfo);

    //create index buffer
    indexBuffer = engine->CreateBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    AllocatedBuffer staging = engine->CreateBuffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    VmaAllocationInfo info;
    vmaGetAllocationInfo(engine->m_device.m_allocator, staging.allocation, &info);
    Vertex* vertices = (Vertex*)info.pMappedData;
    ////////////////////////////////////////////////////////////////

    bool missesTangents = false;
    int verticesProcessed = 0;

    for (const auto& prim : loadedMesh.primitives)
    {
        // Create pointers up front so they can be reused
        const auto& attribs = prim.attributes;
        const tinygltf::Accessor* accessor = nullptr;
        const tinygltf::BufferView* view = nullptr;
        const tinygltf::Buffer* buffer = nullptr;
        int accessorIdx = 0;

        // Indices ---------------------------------------------------------------------
        accessor = &loaded.accessors[prim.indices];
        view = &loaded.bufferViews[accessor->bufferView];
        buffer = &loaded.buffers[view->buffer];
        const uint8_t* indexData = buffer->data.data() + view->byteOffset + accessor->byteOffset;
        uint32_t indexCount = accessor->count, vertexCount = accessor->count;

        // Temporarily convert all the indices to uint32_t
        std::vector<uint32_t> indices(indexCount);
        for (size_t i = 0; i < vertexCount; i++)
        {
            switch (accessor->componentType)
            {
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                indices[i] = static_cast<uint32_t>(*(uint8_t*)(indexData + i * sizeof(uint8_t)));
                break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                indices[i] = static_cast<uint32_t>(*(uint16_t*)(indexData + i * sizeof(uint16_t)));
                break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                indices[i] = static_cast<uint32_t>(*(uint32_t*)(indexData + i * sizeof(uint32_t)));
                break;
            default:
                printf("Unsupported index component type\n");
                break;
            }
        }

        // Create new primitive
        Primitive& newPrim = primitives.emplace_back();
        newPrim.baseVertex = verticesProcessed;
        newPrim.vertexCount = vertexCount;

        // Positions -------------------------------------------------------------------
        accessorIdx = attribs.at("POSITION");
        accessor = &loaded.accessors[accessorIdx];
        view = &loaded.bufferViews[accessor->bufferView];
        buffer = &loaded.buffers[view->buffer];
        const uint8_t* positionData = buffer->data.data() + view->byteOffset + accessor->byteOffset;
        assert(accessor->type == TINYGLTF_TYPE_VEC3);

        // Convert positionData to my format
        for (size_t i = 0; i < indexCount; i++)
        {
            float* pos = (float*)(positionData + indices[i] * sizeof(float) * 3);
            vertices[verticesProcessed + i].position = glm::vec3(pos[0], pos[1], pos[2]);
        }

        // Normals ---------------------------------------------------------------------
        auto it = attribs.find("NORMAL");
        if (it != attribs.end())
        {
            accessorIdx = it->second;
            accessor = &loaded.accessors[accessorIdx];
            view = &loaded.bufferViews[accessor->bufferView];
            buffer = &loaded.buffers[view->buffer];
            const uint8_t* normalData = buffer->data.data() + view->byteOffset + accessor->byteOffset;
            assert(accessor->type == TINYGLTF_TYPE_VEC3);

            // Convert normalData to my format
            for (size_t i = 0; i < indexCount; i++)
            {
                float* normal = (float*)(normalData + indices[i] * sizeof(float) * 3);
                vertices[verticesProcessed + i].normal = glm::vec3(normal[0], normal[1], normal[2]);
            }
        }
        else
        {
            // If normals are not provided, they must be flat
            for (int i = 0; i < vertexCount; i += 3)
            {
                auto& v0 = vertices[i + 0];
                auto& v1 = vertices[i + 1];
                auto& v2 = vertices[i + 2];
                glm::vec3 edge0 = v1.position - v0.position;
                glm::vec3 edge1 = v2.position - v0.position;
                glm::vec4 normal = glm::vec4(glm::normalize(glm::cross(edge0, edge1)), 0.0f);
                v0.normal = normal;
                v1.normal = normal;
                v2.normal = normal;
            }
        }

        // Texture coordinates -------------------------------------------------------
        it = attribs.find("TEXCOORD_0");
        if (it != attribs.end())
        {
            accessorIdx = it->second;
            accessor = &loaded.accessors[accessorIdx];
            view = &loaded.bufferViews[accessor->bufferView];
            buffer = &loaded.buffers[view->buffer];
            const uint8_t* texCoordData = buffer->data.data() + view->byteOffset + accessor->byteOffset;
            assert(accessor->type == TINYGLTF_TYPE_VEC2);

            // Convert texCoordData to my format
            for (size_t i = 0; i < indexCount; i++)
            {
                float* texCoord = (float*)(texCoordData + indices[i] * sizeof(float) * 2);
                vertices[verticesProcessed + i].uvX = texCoord[0];
                vertices[verticesProcessed + i].uvY = texCoord[1];
            }
        }

        // Tangents ------------------------------------------------------------------
        it = attribs.find("TANGENT");
        if (it != attribs.end())
        {
            accessorIdx = it->second;
            accessor = &loaded.accessors[accessorIdx];
            view = &loaded.bufferViews[accessor->bufferView];
            buffer = &loaded.buffers[view->buffer];
            const uint8_t* tangentData = buffer->data.data() + view->byteOffset + accessor->byteOffset;
            assert(accessor->type == TINYGLTF_TYPE_VEC4);

            for (size_t i = 0; i < indexCount; i++)
            {
                float* tangent = (float*)(tangentData + indices[i] * sizeof(float) * 4);
                vertices[verticesProcessed + i].tangent = glm::vec4(tangent[0], tangent[1], tangent[2], tangent[3]);
            }
        }
        else
        {
            // When tangents are not provided, SHOULD compute using MikkTSpace
            missesTangents = true;
        }

        // Copying all indexed positions to the large position buffer,
        // so we have to keep track of how many vertices are already
        // processed for correct indexing
        verticesProcessed += vertexCount;

        // Materials -----------------------------------------------------------------
        int matIdx = prim.material;
        if (matIdx >= 0)
        {
            newPrim.materialId = materials.size();
            auto& newMaterial = materials.emplace_back();
            auto& material = loaded.materials[matIdx];
            LoadMaterial(loaded, material, folder, newMaterial);
        }
    }

    LoadingMesh loadingMesh{};
    loadingMesh.vertexCount = vertexCount;
    loadingMesh.vertices = vertices;
    
    if (missesTangents)
    {
        Model::MikkTSpaceCalc(&loadingMesh);
    }

    // NEW ///////////////////////////////////////////////////////
    VkCommandBuffer cmd;
    engine->m_device.BeginOneTimeSubmit(cmd);

    VkBufferCopy vertexCopy{ 0 };
    vertexCopy.dstOffset = 0;
    vertexCopy.srcOffset = 0;
    vertexCopy.size = vertexBufferSize;

    vkCmdCopyBuffer(cmd, staging.buffer, vertexBuffer.buffer, 1, &vertexCopy);

    VkBufferCopy indexCopy{ 0 };
    indexCopy.dstOffset = 0;
    indexCopy.srcOffset = vertexBufferSize;
    indexCopy.size = indexBufferSize;

    vkCmdCopyBuffer(cmd, staging.buffer, indexBuffer.buffer, 1, &indexCopy);

	engine->m_device.EndOneTimeSubmit();

    engine->DestroyBuffer(staging);
    //////////////////////////////////////////////////////////////
}

void trayser::Mesh::LoadMaterial(const tinygltf::Model& model, const tinygltf::Material& material, const std::string& folder, Material& newMaterial)
{
    auto GetImagePath = [&](const tinygltf::Image& image, const tinygltf::Texture& texture)
        { return folder + (image.uri.empty() ? "_texture" + std::to_string(texture.source) : image.uri); };

    auto& engine = g_engine;

    // Base color -------------------------------------------------------------------
    int texIdx = material.pbrMetallicRoughness.baseColorTexture.index;
    if (texIdx >= 0)
    {
        auto& texture = model.textures[texIdx];
        if (texture.source >= 0)
        {
            auto& image = model.images[texture.source];
            std::string path = GetImagePath(image, texture);
            newMaterial.baseColor = engine.m_resources.Create<Image>(
                path,
                path,
                model,
                image,
                VK_FORMAT_R8G8B8A8_SRGB,
                VK_IMAGE_USAGE_SAMPLED_BIT);
            newMaterial.baseColorFactor = glm::make_vec4(material.pbrMetallicRoughness.baseColorFactor.data());
        }
    }
    // Normal map -------------------------------------------------------------------
    texIdx = material.normalTexture.index;
    if (texIdx >= 0)
    {
        auto& texture = model.textures[texIdx];
        if (texture.source >= 0)
        {
            auto& image = model.images[texture.source];
            std::string path = GetImagePath(image, texture);
            newMaterial.normalMap = engine.m_resources.Create<Image>(
                path,
                path,
                model,
                image,
                VK_FORMAT_R8G8B8A8_UNORM,
                VK_IMAGE_USAGE_SAMPLED_BIT);
        }
    }
    // Metallic roughness -------------------------------------------------------------------
    texIdx = material.pbrMetallicRoughness.metallicRoughnessTexture.index;
    if (texIdx >= 0)
    {
        auto& texture = model.textures[texIdx];
        if (texture.source >= 0)
        {
            auto& image = model.images[texture.source];
            std::string path = GetImagePath(image, texture);
            newMaterial.metallicRoughness = engine.m_resources.Create<Image>(
                path,
                path,
                model,
                image,
                VK_FORMAT_R8G8B8A8_UNORM,
                VK_IMAGE_USAGE_SAMPLED_BIT);
            newMaterial.metallicRoughnessAoFactor.r = material.pbrMetallicRoughness.metallicFactor;
            newMaterial.metallicRoughnessAoFactor.g = material.pbrMetallicRoughness.roughnessFactor;
        }
    }
    // Ambient occlusion -------------------------------------------------------------------
    texIdx = material.occlusionTexture.index;
    if (texIdx >= 0)
    {
        auto& texture = model.textures[texIdx];
        if (texture.source >= 0)
        {
            auto& image = model.images[texture.source];
            std::string path = GetImagePath(image, texture);
            newMaterial.occlusion = engine.m_resources.Create<Image>(
                path,
                path,
                model,
                image,
                VK_FORMAT_R8G8B8A8_UNORM,
                VK_IMAGE_USAGE_SAMPLED_BIT);
            newMaterial.metallicRoughnessAoFactor.b = material.occlusionTexture.strength;
        }
    }
    // Emissive ---------------------------------------------------------------------------
    texIdx = material.emissiveTexture.index;
    if (texIdx >= 0)
    {
        auto& texture = model.textures[texIdx];
        if (texture.source >= 0)
        {
            auto& image = model.images[texture.source];
            std::string path = GetImagePath(image, texture);
            newMaterial.emissive = engine.m_resources.Create<Image>(
                path,
                path,
                model,
                image,
                VK_FORMAT_R8G8B8A8_SRGB,
                VK_IMAGE_USAGE_SAMPLED_BIT);
            newMaterial.emissiveFactor = glm::make_vec3(material.emissiveFactor.data());
        }
    }
}

void trayser::Model::MikkTSpaceCalc(LoadingMesh* mesh)
{
    Model::g_mikkTSpaceCtx.m_pUserData = mesh;
    genTangSpaceDefault(&Model::g_mikkTSpaceCtx);
}

int trayser::Model::MikkTSpaceGetNumFaces(const SMikkTSpaceContext* context)
{
    LoadingMesh* mesh = static_cast<LoadingMesh*>(context->m_pUserData);
    float fSize = (float)mesh->vertexCount / 3.f;
    int iSize = (int)mesh->vertexCount / 3;
    assert((fSize - (float)iSize) == 0.f);
    return iSize;
}

int trayser::Model::MikkTSpaceGetNumVerticesOfFace(const SMikkTSpaceContext* context, const int iFace)
{
    return 3;
}

void trayser::Model::MikkTSpaceGetPosition(const SMikkTSpaceContext* context,
    float* outPos,
    const int iFace,
    const int iVert)
{
    auto mesh = (LoadingMesh*)context->m_pUserData;
    auto index = MikkTSpaceGetVertexIndex(context, iFace, iVert);
    auto vertices = mesh->vertices;

    outPos[0] = vertices[index].position.x;
    outPos[1] = vertices[index].position.y;
    outPos[2] = vertices[index].position.z;
}

void trayser::Model::MikkTSpaceGetNormal(const SMikkTSpaceContext* context,
    float* outNormal,
    const int iFace,
    const int iVert)
{
    auto mesh = (LoadingMesh*)context->m_pUserData;
    auto index = MikkTSpaceGetVertexIndex(context, iFace, iVert);
    auto vertices = mesh->vertices;

    outNormal[0] = vertices[index].normal.x;
    outNormal[1] = vertices[index].normal.y;
    outNormal[2] = vertices[index].normal.z;
}

void trayser::Model::MikkTSpaceGetTexCoords(const SMikkTSpaceContext* context,
    float* outUv,
    const int iFace,
    const int iVert)
{
    auto mesh = (LoadingMesh*)context->m_pUserData;
    auto index = MikkTSpaceGetVertexIndex(context, iFace, iVert);
    auto vertices = mesh->vertices;

    outUv[0] = vertices[index].uvX;
    outUv[1] = vertices[index].uvY;
}

void trayser::Model::MikkTSpaceSetTSpaceBasic(
    const SMikkTSpaceContext* context,
    const float* tangentu,
    const float fSign,
    const int iFace,
    const int iVert)
{
    auto mesh = (LoadingMesh*)context->m_pUserData;
    auto index = MikkTSpaceGetVertexIndex(context, iFace, iVert);
    auto vertices = mesh->vertices;

    vertices[index].tangent.x = tangentu[0];
    vertices[index].tangent.y = tangentu[1];
    vertices[index].tangent.z = tangentu[2];
    vertices[index].tangent.w = -fSign;
}

int trayser::Model::MikkTSpaceGetVertexIndex(const SMikkTSpaceContext* context, int iFace, int iVert)
{
    return iFace * MikkTSpaceGetNumVerticesOfFace(context, iFace) + iVert;
}

void trayser::Model::MikkTSpaceInit()
{
    g_mikkTSpaceIface.m_getNumFaces = MikkTSpaceGetNumFaces;
    g_mikkTSpaceIface.m_getNumVerticesOfFace = MikkTSpaceGetNumVerticesOfFace;
    g_mikkTSpaceIface.m_getNormal = MikkTSpaceGetNormal;
    g_mikkTSpaceIface.m_getPosition = MikkTSpaceGetPosition;
    g_mikkTSpaceIface.m_getTexCoord = MikkTSpaceGetTexCoords;
    g_mikkTSpaceIface.m_setTSpaceBasic = MikkTSpaceSetTSpaceBasic;
    g_mikkTSpaceCtx.m_pInterface = &g_mikkTSpaceIface;
}

static bool LoadImageCallback(
    tinygltf::Image* img,
    const int imgIdx,
    std::string* rootDir,
    std::string* fileName,
    int reqWidth,
    int reqHeight,
    const unsigned char* bytes,
    int size,
    void* userData)
{
    // Do nothing. Logic resides in the model loader
    return true;
}

trayser::Model::Model(std::string_view path, Engine* engine)
{
    tinygltf::TinyGLTF loader;
    loader.SetImageLoader(LoadImageCallback, nullptr);

    tinygltf::Model loaded;
    std::string error;
    std::string warning;

    std::string folder = std::filesystem::path(path).parent_path().string() + "/";
    std::filesystem::path fsPath = { path };

    bool ret = false;
    if (fsPath.extension() == ".gltf")
    {
        ret = loader.LoadASCIIFromFile(&loaded, &error, &warning, std::string(path));
    }
    else if (fsPath.extension() == ".glb")
    {
        ret = loader.LoadBinaryFromFile(&loaded, &error, &warning, std::string(path));
    }
    else
    {
        printf("Error: engine does not support this file format");
        return;
    }

    if (!warning.empty()) printf("Warning: %s\n", warning.c_str());
    if (!error.empty()) printf("Error: %s\n", error.c_str());
    if (!ret)
    {
        printf("Failed to parse glTF\n");
        return;
    }

    printf("File loaded!");

    // Pre allocate the right amount of nodes
    const auto& scene = loaded.scenes[loaded.defaultScene];
    nodes.reserve(loaded.nodes.size());

    // Recurse through model nodes
    for (const auto& nodeIdx : scene.nodes)
    {
        const auto& loadedNode = loaded.nodes[nodeIdx];
        TraverseNode(loaded, loadedNode, nullptr, folder);
    }
}

trayser::Image::Image(const std::string& path, const tinygltf::Model& model, const tinygltf::Image& inImage, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
    auto& engine = g_engine;

    int width, height, channels;
    uint8_t* data = nullptr;

    if (inImage.bufferView > -1)
    {   // Is embedded
        const tinygltf::BufferView& view = model.bufferViews[inImage.bufferView];
        const tinygltf::Buffer& buffer = model.buffers[view.buffer];
        const unsigned char* file = buffer.data.data() + view.byteOffset;
        data = stbi_load_from_memory(file, view.byteLength, &width, &height, &channels, 4);
    }
    else
    {   // Is external, use the path
        data = stbi_load(path.c_str(), &width, &height, &channels, 4);
    }

    if (!data)
    {
        printf("Warning: Failed to load image\n");
        return;
    }

    size_t data_size = width * height * 4;
    AllocatedBuffer uploadbuffer = engine.CreateBuffer(data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    memcpy(uploadbuffer.info.pMappedData, data, data_size);

    VkExtent3D extent = { width, height, 1 };

    AllocatedImage new_image = engine.CreateImage(extent, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped);

    VkCommandBuffer cmd;
    engine.m_device.BeginOneTimeSubmit(cmd);

    vkutil::TransitionImage(cmd, new_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkBufferImageCopy copyRegion = {};
    copyRegion.bufferOffset = 0;
    copyRegion.bufferRowLength = 0;
    copyRegion.bufferImageHeight = 0;

    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent = extent;

    // copy the buffer into the image
    vkCmdCopyBufferToImage(cmd, uploadbuffer.buffer, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
        &copyRegion);

    vkutil::TransitionImage(cmd, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	engine.m_device.EndOneTimeSubmit();

    engine.DestroyBuffer(uploadbuffer);
    stbi_image_free(data);

    image = new_image.image;
    imageView = new_image.imageView;
    allocation = new_image.allocation;
    imageExtent = new_image.imageExtent;
    imageFormat = new_image.imageFormat;
}

trayser::Image::Image(u32* data, u32 width, u32 height, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
    auto& engine = g_engine;

    size_t data_size = width * height * 4;
    AllocatedBuffer uploadbuffer = engine.CreateBuffer(data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    memcpy(uploadbuffer.info.pMappedData, data, data_size);

    VkExtent3D extent = { width, height, 1 };

    AllocatedImage new_image = engine.CreateImage(extent, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped);

    VkCommandBuffer cmd;
    engine.m_device.BeginOneTimeSubmit(cmd);

    vkutil::TransitionImage(cmd, new_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkBufferImageCopy copyRegion = {};
    copyRegion.bufferOffset = 0;
    copyRegion.bufferRowLength = 0;
    copyRegion.bufferImageHeight = 0;

    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent = extent;

    // copy the buffer into the image
    vkCmdCopyBufferToImage(cmd, uploadbuffer.buffer, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
        &copyRegion);

    vkutil::TransitionImage(cmd, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  
		engine.m_device.EndOneTimeSubmit();

    engine.DestroyBuffer(uploadbuffer);

    image = new_image.image;
    imageView = new_image.imageView;
    allocation = new_image.allocation;
    imageExtent = new_image.imageExtent;
    imageFormat = new_image.imageFormat;
}
