#include <pch.h>
#include <loader.h>

#include "stb_image.h"
#include <iostream>
#include <assert.h>
#include <fstream>

#include "engine.h"
#include <device.h>
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
        //node.mesh = engine.m_resources.Create<Mesh>(name, &engine, loaded, loadedMesh, folder);
        node.handle = engine.m_meshPool.Create(name, &engine, loaded, loadedMesh, folder);
    }

    for (int nodeIdx : loadedNode.children)
    {
        TraverseNode(loaded, loaded.nodes[nodeIdx], &node, folder);
    }
}

trayser::Mesh::~Mesh()
{
    g_engine.m_device.m_rtFuncs.vkDestroyAccelerationStructureKHR(g_engine.m_device.m_device, BLas.accel, nullptr);
    vmaDestroyBuffer(g_engine.m_device.m_allocator, BLas.buffer.buffer, BLas.buffer.allocation);
}

trayser::Mesh::Mesh(Engine* engine, tinygltf::Model& loaded, const tinygltf::Mesh& loadedMesh, const std::string& folder)
{
    u32 primitiveCount = loadedMesh.primitives.size();

    // Indexing is already being processed during loading,
    // so the amount of indices equals the amount of vertices
    indexCount = Model::TotalIndexCountInMesh(loaded, loadedMesh);
    if (indexCount == 0)
    {
        printf("Model loader: mesh misses indices");
        return;
    }

    vertexCount = Model::TotalVertexCountInMesh(loaded, loadedMesh);
    if (vertexCount == 0)
    {
        printf("Model loader: mesh misses vertices");
        return;
    }

    // NEW /////////////////////////////////////////////////////////
    const size_t vertexBufferSize = vertexCount * sizeof(Vertex);
    const size_t indexBufferSize = indexCount * sizeof(u32);
    const size_t primitiveBufferSize = primitiveCount * sizeof(gpu::Primitive);

    //create vertex buffer
    engine->m_device.CreateBuffer(
        vertexBufferSize, 
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | 
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | 
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | 
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | 
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY,
        { 0 },
        vertexBuffer);

    //create index buffer
    engine->m_device.CreateBuffer(
        indexBufferSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | 
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | 
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | 
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY,
        { 0 },
        indexBuffer);

    //create primitive buffer
    engine->m_device.CreateBuffer(
        indexBufferSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | 
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | 
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | 
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY,
        { 0 },
        primitiveBuffer);

    //find the adress of the vertex and index buffer
    vertexBufferAddr    = engine->m_device.GetBufferDeviceAddress(vertexBuffer);
    indexBufferAddr     = engine->m_device.GetBufferDeviceAddress(indexBuffer);
    primitiveBufferAddr = engine->m_device.GetBufferDeviceAddress(primitiveBuffer);

    Device::StageBuffer vertexStage, indexStage, primStage;
    engine->m_device.CreateStageBuffer(vertexBufferSize, vertexStage);
    engine->m_device.CreateStageBuffer(indexBufferSize, indexStage);
    engine->m_device.CreateStageBuffer(primitiveBufferSize, primStage);

    Vertex* vertices                = (Vertex*)vertexStage.mapped;
    u32* indices                    = (u32*)indexStage.mapped;
    gpu::Primitive* primitivesAddr  = (gpu::Primitive*)primStage.mapped;
    ////////////////////////////////////////////////////////////////

    bool missesTangents = false;
    int verticesProcessed = 0;
    int indicesProcessed = 0;

    int i = 0;
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
        uint32_t indexCount = accessor->count;

        // Temporarily convert all the indices to uint32_t
        for (size_t i = 0; i < indexCount; i++)
        {
            switch (accessor->componentType)
            {
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                indices[indicesProcessed + i] = static_cast<uint32_t>(*(uint8_t*)(indexData + i * sizeof(uint8_t)));
                break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                indices[indicesProcessed + i] = static_cast<uint32_t>(*(uint16_t*)(indexData + i * sizeof(uint16_t)));
                break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                indices[indicesProcessed + i] = static_cast<uint32_t>(*(uint32_t*)(indexData + i * sizeof(uint32_t)));
                break;
            default:
                printf("Unsupported index component type\n");
                break;
            }
        }

        // Positions -------------------------------------------------------------------
        accessorIdx = attribs.at("POSITION");
        accessor = &loaded.accessors[accessorIdx];
        view = &loaded.bufferViews[accessor->bufferView];
        buffer = &loaded.buffers[view->buffer];
        const uint8_t* positionData = buffer->data.data() + view->byteOffset + accessor->byteOffset;
        uint32_t vertexCount = accessor->count;
        assert(accessor->type == TINYGLTF_TYPE_VEC3);

        // Create new primitive
        Primitive& newPrim = primitives.emplace_back();
        newPrim.baseVertex = verticesProcessed;
        newPrim.vertexCount = vertexCount;
        newPrim.baseIndex = indicesProcessed;
		newPrim.indexCount = indexCount;

        // Convert normalData to my format
        for (size_t i = 0; i < vertexCount; i++)
        {
            float* pos = (float*)(positionData + i * sizeof(float) * 3);
            vertices[verticesProcessed + i].position = glm::vec4(pos[0], pos[1], pos[2], 1.0);
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
            for (size_t i = 0; i < vertexCount; i++)
            {
                float* normal = (float*)(normalData + i * sizeof(float) * 3);
                vertices[verticesProcessed + i].normal = glm::vec4(normal[0], normal[1], normal[2], 0.0);
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
            for (size_t i = 0; i < vertexCount; i++)
            {
                float* texCoord = (float*)(texCoordData + i * sizeof(float) * 2);
                vertices[verticesProcessed + i].texCoord.x = texCoord[0];
                vertices[verticesProcessed + i].texCoord.y = texCoord[1];
                vertices[verticesProcessed + i].texCoord.z = 0.0;
                vertices[verticesProcessed + i].texCoord.w = 0.0;
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

            for (size_t i = 0; i < vertexCount; i++)
            {
                float* tangent = (float*)(tangentData + i * sizeof(float) * 4);
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
		indicesProcessed += indexCount;

        // Materials -----------------------------------------------------------------
        int matIdx = prim.material;
        newPrim.materialId = LoadMaterial(loaded, matIdx, folder);
        primitivesAddr[i].materialHandle = newPrim.materialId;
        primitivesAddr[i].baseVertex = newPrim.baseVertex;
        primitivesAddr[i].vertexCount = newPrim.vertexCount;
        primitivesAddr[i].baseIndex = newPrim.baseIndex;
        primitivesAddr[i].indexCount = newPrim.indexCount;
        i++;
    }

    
    if (missesTangents)
    {
        LoadingMesh loadingMesh{};
        loadingMesh.vertexCount = vertexCount;
        loadingMesh.indexCount = indexCount;
        loadingMesh.vertices = vertices;
        loadingMesh.indices = indices;

        Model::MikkTSpaceCalc(&loadingMesh);
    }

    // NEW ///////////////////////////////////////////////////////
    VkCommandBuffer cmd;
    engine->m_device.BeginOneTimeSubmit(cmd);

    VkBufferCopy vertexCopy{ 0 };
    vertexCopy.dstOffset = 0;
    vertexCopy.srcOffset = 0;
    vertexCopy.size = vertexBufferSize;
    vkCmdCopyBuffer(cmd, vertexStage.buffer, vertexBuffer.buffer, 1, &vertexCopy);

    VkBufferCopy indexCopy{ 0 };
    indexCopy.dstOffset = 0;
    indexCopy.srcOffset = 0;
    indexCopy.size = indexBufferSize;
    vkCmdCopyBuffer(cmd, indexStage.buffer, indexBuffer.buffer, 1, &indexCopy);

    VkBufferCopy primitiveCopy{ 0 };
    primitiveCopy.dstOffset = 0;
    primitiveCopy.srcOffset = 0;
    primitiveCopy.size = primitiveBufferSize;
    vkCmdCopyBuffer(cmd, primStage.buffer, primitiveBuffer.buffer, 1, &primitiveCopy);

	engine->m_device.EndOneTimeSubmit();

    engine->m_device.DestroyStageBuffer(vertexStage);
    engine->m_device.DestroyStageBuffer(indexStage);
    engine->m_device.DestroyStageBuffer(primStage);
    //////////////////////////////////////////////////////////////

    InitBLas();
}

trayser::MaterialHandle trayser::Mesh::LoadMaterial(const tinygltf::Model& model, int matIdx, const std::string& folder)
{
    if (matIdx < 0)
        return ResourceHandle_Invalid;

    auto GetMaterialPath = [&](int matIdx)
        { return folder + ("_material" + matIdx); };

    auto& engine = g_engine;

    return engine.m_materialPool.Create(GetMaterialPath(matIdx), model, model.materials[matIdx], folder);
}

void trayser::Mesh::PrimitivesToGeometries(std::vector<VkAccelerationStructureGeometryKHR>& geometries,
    std::vector<VkAccelerationStructureBuildRangeInfoKHR>& rangeInfos)
{
    geometries.clear();
    rangeInfos.clear();
    geometries.reserve(primitives.size());
    rangeInfos.reserve(primitives.size());

    for (auto& prim : primitives)
    {
        auto& geometry = geometries.emplace_back();
        auto& rangeInfo = rangeInfos.emplace_back();

        VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
        triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        triangles.vertexData = { .deviceAddress = vertexBufferAddr + offsetof(Vertex, position) };
        triangles.vertexStride = sizeof(Vertex);
        triangles.maxVertex = vertexCount - 1;
        triangles.indexType = VK_INDEX_TYPE_UINT32;
        triangles.indexData = { .deviceAddress = indexBufferAddr };

        geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        geometry.flags = VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR | VK_GEOMETRY_OPAQUE_BIT_KHR;
        geometry.geometry.triangles = triangles;

        rangeInfo.primitiveCount = prim.indexCount / 3;
        rangeInfo.primitiveOffset = prim.baseIndex * sizeof(uint32_t);
        rangeInfo.firstVertex = prim.baseVertex;
        rangeInfo.transformOffset = 0;
    }
}

void trayser::Mesh::InitBLas()
{
    std::vector<VkAccelerationStructureGeometryKHR> geometries;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> rangeInfos;

    PrimitivesToGeometries(geometries, rangeInfos);

    g_engine.m_device.CreateAccelerationStructure2(VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, BLas, geometries,
        rangeInfos, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
}

void trayser::Model::MikkTSpaceCalc(LoadingMesh* mesh)
{
    Model::g_mikkTSpaceCtx.m_pUserData = mesh;
    genTangSpaceDefault(&Model::g_mikkTSpaceCtx);
}

int trayser::Model::MikkTSpaceGetNumFaces(const SMikkTSpaceContext* context)
{
    LoadingMesh* mesh = static_cast<LoadingMesh*>(context->m_pUserData);
    float fSize = (float)mesh->indexCount / 3.f;
    int iSize = (int)mesh->indexCount / 3;
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

    outUv[0] = vertices[index].texCoord.x;
    outUv[1] = vertices[index].texCoord.y;
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
    auto mesh = static_cast<LoadingMesh*>(context->m_pUserData);
    return mesh->indices[iFace * 3 + iVert];
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

trayser::Texture::~Texture()
{
    g_engine.m_device.DestroyImage(image);
}

trayser::Texture::Texture(const std::string& path, const tinygltf::Model& model, const tinygltf::Image& inImage, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
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
    //AllocatedBuffer uploadbuffer = engine.CreateBuffer(data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    Device::Buffer uploadBuffer;
    VmaAllocationInfo uploadBufferInfo;
    VK_CHECK(engine.m_device.CreateBuffer(data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT, uploadBuffer, &uploadBufferInfo));

    memcpy(uploadBufferInfo.pMappedData, data, data_size);

    VkExtent3D extent = { width, height, 1 };

    Device::Image newImage{};
    VkImageCreateInfo imageCreateInfo = ImageCreateInfo();
    imageCreateInfo.format      = format;
    imageCreateInfo.usage       = usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageCreateInfo.extent      = extent;
    imageCreateInfo.imageType   = VK_IMAGE_TYPE_2D;
    imageCreateInfo.mipLevels   = GetMaxMipLevels(extent.width, extent.height);
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples     = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling      = VK_IMAGE_TILING_OPTIMAL;

    // always allocate images on dedicated GPU memory
    VmaAllocationCreateInfo allocinfo = {};
    allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // allocate and create the image
    VK_CHECK(vmaCreateImage(g_engine.m_device.m_allocator, &imageCreateInfo, &allocinfo, &newImage.image, &newImage.allocation, nullptr));

    VkCommandBuffer cmd;
    engine.m_device.BeginOneTimeSubmit(cmd);

    vkutil::TransitionImage(cmd, newImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 1);

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
    vkCmdCopyBufferToImage(cmd, uploadBuffer.buffer, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
        &copyRegion);

    vkutil::TransitionImage(cmd, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 1);

    engine.m_device.EndOneTimeSubmit();

    engine.m_device.DestroyBuffer(uploadBuffer);
    stbi_image_free(data);

    g_engine.m_device.BeginOneTimeSubmit(cmd);

    // Transition image to layout in which we can perform mip map generation
    vkutil::TransitionImage(cmd, newImage.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 1);

    for (uint32_t mip = 1; mip < imageCreateInfo.mipLevels; ++mip)
    {
        vkutil::TransitionImage(cmd, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, mip - 1, 1);
        vkutil::TransitionImage(cmd, newImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mip, 1);

        VkImageBlit blit = {};
        blit.srcSubresource.aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel        = mip - 1;
        blit.srcSubresource.baseArrayLayer  = 0;
        blit.srcSubresource.layerCount      = 1;
        blit.srcOffsets[1] = { static_cast<int32_t>(extent.width >> (mip - 1)), static_cast<int32_t>(extent.height >> (mip - 1)), 1 }; // src extent

        blit.dstSubresource.aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel        = mip;
        blit.dstSubresource.baseArrayLayer  = 0;
        blit.dstSubresource.layerCount      = 1;
        blit.dstOffsets[1] = { static_cast<int32_t>(extent.width >> mip), static_cast<int32_t>(extent.height >> mip), 1 }; // dst extent

        vkCmdBlitImage(
            cmd,
            newImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit, VK_FILTER_LINEAR
        );
    }

    vkutil::TransitionImage(cmd, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, imageCreateInfo.mipLevels - 1);

    // Transition back to usable format
    vkutil::TransitionImage(cmd, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, imageCreateInfo.mipLevels - 1, 1);

    g_engine.m_device.EndOneTimeSubmit();

    // build a image-view for the image
    VkImageViewCreateInfo viewCreateInfo = ImageViewCreateInfo();
    viewCreateInfo.viewType                         = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format                           = format;
    viewCreateInfo.image                            = newImage.image;
    viewCreateInfo.subresourceRange.baseMipLevel    = 0;
    viewCreateInfo.subresourceRange.levelCount      = VK_REMAINING_MIP_LEVELS;
    viewCreateInfo.subresourceRange.baseArrayLayer  = 0;
    viewCreateInfo.subresourceRange.layerCount      = VK_REMAINING_ARRAY_LAYERS;
    viewCreateInfo.subresourceRange.aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT;
    VK_CHECK(vkCreateImageView(g_engine.m_device.m_device, &viewCreateInfo, nullptr, &imageView));

    image.image         = newImage.image;
    image.allocation    = newImage.allocation;
}

//trayser::Texture::Texture(const std::string& path, VkFormat format, VkImageUsageFlags usage)
//{
//    std::filesystem::path fsPath = path;
//    if (fsPath.extension() == ".hdr")
//    {
//        int width, height, channels;
//        float* stbiData = stbi_loadf(path.c_str(), &width, &height, &channels, 4);
//        if (!stbiData)
//            return;
//
//        size_t sizeBytes = width * height * 4;
//        AllocatedBuffer staging = g_engine.CreateBuffer(sizeBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
//        memcpy(staging.info.pMappedData, stbiData, sizeBytes);
//
//        VkExtent3D extent = { width, height, 1 };
//
//        AllocatedImage equiImage{};
//        {
//        equiImage.imageFormat = format;
//        equiImage.imageExtent = extent;
//
//        VkImageCreateInfo imageCreateInfo = ImageCreateInfo();
//        imageCreateInfo.imageType   = VK_IMAGE_TYPE_2D;
//        imageCreateInfo.format      = VK_FORMAT_R16G16B16A16_SFLOAT;
//        imageCreateInfo.extent      = extent;
//        imageCreateInfo.usage       = usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
//        imageCreateInfo.mipLevels   = 1;
//        imageCreateInfo.arrayLayers = 1;
//        imageCreateInfo.samples     = VK_SAMPLE_COUNT_1_BIT;
//        imageCreateInfo.tiling      = VK_IMAGE_TILING_OPTIMAL;
//
//        // Always allocate images on dedicated GPU memory
//        VmaAllocationCreateInfo allocInfo = {};
//        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
//        allocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
//        VK_CHECK(vmaCreateImage(g_engine.m_device.m_allocator, &imageCreateInfo, &allocInfo, &equiImage.image, &equiImage.allocation, nullptr));
//
//        // Build a image-view for the image
//        VkImageViewCreateInfo viewCreateInfo = ImageViewCreateInfo();
//        viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
//        viewCreateInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
//        viewCreateInfo.image = equiImage.image;
//        viewCreateInfo.subresourceRange.baseMipLevel = 0;
//        viewCreateInfo.subresourceRange.levelCount = 1;
//        viewCreateInfo.subresourceRange.baseArrayLayer = 0;
//        viewCreateInfo.subresourceRange.layerCount = 1;
//        viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//        VK_CHECK(vkCreateImageView(g_engine.m_device.m_device, &viewCreateInfo, nullptr, &equiImage.imageView));
//        }
//
//        AllocatedImage cubeImage{};
//        {
//        cubeImage.imageFormat = format;
//        cubeImage.imageExtent = extent;
//
//        VkImageCreateInfo imageCreateInfo = ImageCreateInfo();
//        imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
//        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
//        imageCreateInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
//        imageCreateInfo.extent = extent;
//        imageCreateInfo.usage = usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
//        imageCreateInfo.mipLevels = 1;
//        imageCreateInfo.arrayLayers = 6; // 6 sides on a cube
//        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
//        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
//
//        // Always allocate images on dedicated GPU memory
//        VmaAllocationCreateInfo allocInfo = {};
//        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
//        allocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
//        VK_CHECK(vmaCreateImage(g_engine.m_device.m_allocator, &imageCreateInfo, &allocInfo, &cubeImage.image, &cubeImage.allocation, nullptr));
//
//        // Build a image-view for the image
//        VkImageViewCreateInfo viewCreateInfo = ImageViewCreateInfo();
//        viewCreateInfo.viewType                         = VK_IMAGE_VIEW_TYPE_CUBE;
//        viewCreateInfo.format                           = VK_FORMAT_R16G16B16A16_SFLOAT;
//        viewCreateInfo.image                            = cubeImage.image;
//        viewCreateInfo.subresourceRange.baseMipLevel    = 0;
//        viewCreateInfo.subresourceRange.levelCount      = 1;
//        viewCreateInfo.subresourceRange.baseArrayLayer  = 0;
//        viewCreateInfo.subresourceRange.layerCount      = 6;
//        viewCreateInfo.subresourceRange.aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT;
//        VK_CHECK(vkCreateImageView(g_engine.m_device.m_device, &viewCreateInfo, nullptr, &cubeImage.imageView));
//        }
//
//        // Copy stbi contents to equi image
//        VkCommandBuffer cmd;
//        g_engine.m_device.BeginOneTimeSubmit(cmd);
//
//        vkutil::TransitionImage(cmd, equiImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
//
//        VkBufferImageCopy copyRegion = {};
//        copyRegion.bufferOffset = 0;
//        copyRegion.bufferRowLength = 0;
//        copyRegion.bufferImageHeight = 0;
//        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//        copyRegion.imageSubresource.mipLevel = 0;
//        copyRegion.imageSubresource.baseArrayLayer = 0;
//        copyRegion.imageSubresource.layerCount = 1;
//        copyRegion.imageExtent = extent;
//        vkCmdCopyBufferToImage(cmd, staging.buffer, equiImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
//
//        vkutil::TransitionImage(cmd, equiImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
//            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
//
//        g_engine.m_device.EndOneTimeSubmit();
//        g_engine.DestroyBuffer(staging);
//
//        stbi_image_free(stbiData);
//
//        image.image       = cubeImage.image;
//        imageView   = cubeImage.imageView;
//        image.allocation  = cubeImage.allocation;
//        imageExtent = cubeImage.imageExtent;
//        imageFormat = cubeImage.imageFormat;
//    }
//}

trayser::Texture::Texture(u32* data, u32 width, u32 height, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
    auto& engine = g_engine;

    size_t data_size = width * height * 4;
    Device::Buffer uploadbuffer;
    VmaAllocationInfo allocInfo{};
    engine.m_device.CreateBuffer(data_size, 
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT, 
        uploadbuffer, &allocInfo);

    memcpy(allocInfo.pMappedData, data, data_size);

    VkExtent3D extent = { width, height, 1 };

    //Device::Image new_image = engine.CreateImage(extent, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped);

    VkImageCreateInfo createInfo = ImageCreateInfo();
    createInfo.format       = format;
    createInfo.usage        = usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    createInfo.extent       = extent;
    createInfo.imageType    = VK_IMAGE_TYPE_2D;
    createInfo.mipLevels    = 1;
    createInfo.arrayLayers  = 1;
    createInfo.samples      = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling       = VK_IMAGE_TILING_OPTIMAL;
    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.flags   = 0;
    allocCreateInfo.usage   = VMA_MEMORY_USAGE_GPU_ONLY;
    Device::Image new_image;
    engine.m_device.CreateImage(createInfo, allocCreateInfo, new_image);

    VkImageViewCreateInfo viewCreateInfo = ImageViewCreateInfo();
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format   = format;
    viewCreateInfo.image    = new_image.image;
    viewCreateInfo.subresourceRange.baseMipLevel = 0;
    viewCreateInfo.subresourceRange.levelCount = 1;
    viewCreateInfo.subresourceRange.baseArrayLayer = 0;
    viewCreateInfo.subresourceRange.layerCount = 1;
    viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewCreateInfo.subresourceRange.levelCount = createInfo.mipLevels;
    VK_CHECK(vkCreateImageView(g_engine.m_device.m_device, &viewCreateInfo, nullptr, &imageView));

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

    engine.m_device.DestroyBuffer(uploadbuffer);

    image.image = new_image.image;
    image.allocation = new_image.allocation;
}

trayser::Texture::Texture(u16* data, u32 width, u32 height, VkFormat format, VkImageUsageFlags usage, HDRI)
{
    auto& engine = g_engine;

    size_t data_size = width * height * 4 * 2;
    Device::Buffer uploadbuffer;
    VmaAllocationInfo allocInfo;
    engine.m_device.CreateBuffer(data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT, uploadbuffer, &allocInfo);
    //engine.CreateBuffer(data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    memcpy(allocInfo.pMappedData, data, data_size);

    VkExtent3D extent = { width, height, 1 };

    //AllocatedImage new_image = engine.CreateImage(extent, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, false);

    VkImageCreateInfo createInfo = ImageCreateInfo();
    createInfo.format = format;
    createInfo.usage = usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    createInfo.extent = extent;
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.mipLevels = 1;
    createInfo.arrayLayers = 1;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.flags = 0;
    allocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    Device::Image new_image;
    engine.m_device.CreateImage(createInfo, allocCreateInfo, new_image);

    VkImageViewCreateInfo viewCreateInfo = ImageViewCreateInfo();
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format = format;
    viewCreateInfo.image = new_image.image;
    viewCreateInfo.subresourceRange.baseMipLevel = 0;
    viewCreateInfo.subresourceRange.levelCount = 1;
    viewCreateInfo.subresourceRange.baseArrayLayer = 0;
    viewCreateInfo.subresourceRange.layerCount = 1;
    viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewCreateInfo.subresourceRange.levelCount = createInfo.mipLevels;
    VK_CHECK(vkCreateImageView(g_engine.m_device.m_device, &viewCreateInfo, nullptr, &imageView));

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

    engine.m_device.DestroyBuffer(uploadbuffer);

    image.image = new_image.image;
    image.allocation = new_image.allocation;
}

trayser::Texture::Texture(f16* data, u32 width, u32 height, VkImageUsageFlags usage, HDRI)
{
    VkDeviceSize sizeBytes = VkDeviceSize{ width * height * 4 * 2 }; // w * h * channels * bytes/pixel

    VkBuffer stagBuffer{};
    VmaAllocation stagAlloc{};
    VmaAllocationInfo stagInfo{};
    {
        VkBufferCreateInfo bufferInfo = BufferCreateInfo();
        bufferInfo.size = sizeBytes;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VK_CHECK(vmaCreateBuffer(g_engine.m_device.m_allocator, &bufferInfo, &allocInfo, &stagBuffer, &stagAlloc, &stagInfo));
    }

    // Copy to stage
    memcpy(stagInfo.pMappedData, data, sizeBytes);

    VkExtent3D extent = { width, height, 1 };

    //AllocatedImage new_image = g_engine.CreateImage(extent, VK_FORMAT_R16G16B16A16_SFLOAT, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, false);

    VkImageCreateInfo createInfo = ImageCreateInfo();
    createInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    createInfo.usage = usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    createInfo.extent = extent;
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.mipLevels = 1;
    createInfo.arrayLayers = 1;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.flags = 0;
    allocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    Device::Image new_image;
    g_engine.m_device.CreateImage(createInfo, allocCreateInfo, new_image);

    VkImageViewCreateInfo viewCreateInfo = ImageViewCreateInfo();
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    viewCreateInfo.image = new_image.image;
    viewCreateInfo.subresourceRange.baseMipLevel = 0;
    viewCreateInfo.subresourceRange.levelCount = 1;
    viewCreateInfo.subresourceRange.baseArrayLayer = 0;
    viewCreateInfo.subresourceRange.layerCount = 1;
    viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewCreateInfo.subresourceRange.levelCount = createInfo.mipLevels;
    VK_CHECK(vkCreateImageView(g_engine.m_device.m_device, &viewCreateInfo, nullptr, &imageView));

    VkCommandBuffer cmd;
    g_engine.m_device.BeginOneTimeSubmit(cmd);

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
    vkCmdCopyBufferToImage(cmd, stagBuffer, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
        &copyRegion);

    vkutil::TransitionImage(cmd, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    g_engine.m_device.EndOneTimeSubmit();

    vmaDestroyBuffer(g_engine.m_device.m_allocator, stagBuffer, image.allocation);

    image.image = new_image.image;
    image.allocation = new_image.allocation;
}

trayser::Material::Material(Default)
{
    {
    uint32_t packedColor = glm::packUnorm4x8(float4(1.0));
    baseColorHandle = g_engine.m_texturePool.Create(
        "default_base_color",
        &packedColor,
        1, // width
        1, // height
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_USAGE_SAMPLED_BIT);
    baseColorFactor = float4(1.0);
    }

    {
    uint32_t packedColor = glm::packUnorm4x8(float4(0.5, 0.5, 1.0, 1.0));
    normalMapHandle = g_engine.m_texturePool.Create(
        "default_normal_map",
        &packedColor,
        1, // width
        1, // height
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT);
    }

    {
    uint32_t packedColor = glm::packUnorm4x8(float4(0.0, 1.0, 1.0, 1.0));
    metalRoughHandle = g_engine.m_texturePool.Create(
        "default_metal_rough",
        &packedColor,
        1, // width
        1, // height
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT);
    metallicRoughnessAoFactor = float4(0.5, 0.5, 0.5, 1.0);
    }

    {
    uint32_t packedColor = glm::packUnorm4x8(float4(1.0));
    aoHandle = g_engine.m_texturePool.Create(
        "default_ambient_occlusion",
        &packedColor,
        1, // width
        1, // height
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT);
    }

    {
    uint32_t packedColor = glm::packUnorm4x8(float4(1.0));
    emissiveHandle = g_engine.m_texturePool.Create(
        "default_emissive",
        &packedColor,
        1, // width
        1, // height
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_USAGE_SAMPLED_BIT);
    emissiveFactor = float4(1.0);
    }
}

trayser::Material::Material(const tinygltf::Model& model, const tinygltf::Material& material, const std::string& folder)
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
            baseColorHandle = engine.m_texturePool.Create(
                path,
                path,
                model,
                image,
                VK_FORMAT_R8G8B8A8_SRGB,
                VK_IMAGE_USAGE_SAMPLED_BIT);
        }
    }
    baseColorFactor = glm::make_vec4(material.pbrMetallicRoughness.baseColorFactor.data());
    // Normal map -------------------------------------------------------------------
    texIdx = material.normalTexture.index;
    if (texIdx >= 0)
    {
        auto& texture = model.textures[texIdx];
        if (texture.source >= 0)
        {
            auto& image = model.images[texture.source];
            std::string path = GetImagePath(image, texture);
            normalMapHandle = engine.m_texturePool.Create(
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
            metalRoughHandle = engine.m_texturePool.Create(
                path,
                path,
                model,
                image,
                VK_FORMAT_R8G8B8A8_UNORM,
                VK_IMAGE_USAGE_SAMPLED_BIT);
        }
    }
    metallicRoughnessAoFactor.r = material.pbrMetallicRoughness.metallicFactor;
    metallicRoughnessAoFactor.g = material.pbrMetallicRoughness.roughnessFactor;
    // Ambient occlusion -------------------------------------------------------------------
    texIdx = material.occlusionTexture.index;
    if (texIdx >= 0)
    {
        auto& texture = model.textures[texIdx];
        if (texture.source >= 0)
        {
            auto& image = model.images[texture.source];
            std::string path = GetImagePath(image, texture);
            aoHandle = engine.m_texturePool.Create(
                path,
                path,
                model,
                image,
                VK_FORMAT_R8G8B8A8_UNORM,
                VK_IMAGE_USAGE_SAMPLED_BIT);
        }
    }
    metallicRoughnessAoFactor.b = material.occlusionTexture.strength;
    // Emissive ---------------------------------------------------------------------------
    texIdx = material.emissiveTexture.index;
    if (texIdx >= 0)
    {
        auto& texture = model.textures[texIdx];
        if (texture.source >= 0)
        {
            auto& image = model.images[texture.source];
            std::string path = GetImagePath(image, texture);
            emissiveHandle = engine.m_texturePool.Create(
                path,
                path,
                model,
                image,
                VK_FORMAT_R8G8B8A8_SRGB,
                VK_IMAGE_USAGE_SAMPLED_BIT);
            if (emissiveHandle == ResourceHandle_Invalid)
            {
                emissiveHandle = 0;
            }
        }
    }
    emissiveFactor = glm::vec4(glm::make_vec3(material.emissiveFactor.data()), 0.0);
}
