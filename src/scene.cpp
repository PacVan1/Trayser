#include <pch.h>

#include <stack>
#include <scene.h>
#include <engine.h>

void trayser::Scene::Init()
{
    m_sceneGraphDirty = true;
    m_TLasDirty = true;
    m_root = CreateNode(entt::null);
    //BuildTLas();
    m_TLasInitialized = false;
}

void trayser::Scene::Destroy()
{
    DestroyTLas();
}

void trayser::Scene::Update(float dt)
{
    if (m_sceneGraphDirty)
        BuildSceneGraph();

    auto view = m_registry.view<LocalTransform, WorldTransform>();

    for (int i = 1; i < m_traversalBuffer.size(); i++)
    {
        Entity child = m_traversalBuffer[i];

        if (child == entt::null)
        {
            i++;
            continue;
        }

        Entity parent = m_traversalBuffer[i - 1];

        const auto& [cLocalTf, cWorldTf] = view.get(child);

        if (cLocalTf.dirty)
        {   // If dirty, update the local matrix
            glm::mat4 T = glm::translate(glm::mat4(1.0f), cLocalTf.translation);
            glm::mat4 R = glm::toMat4(cLocalTf.orientation);
            glm::mat4 S = glm::scale(glm::mat4(1.0f), cLocalTf.scale);
            cLocalTf.matrix = T * R * S;

            if (parent == entt::null)
            {
                // If child has no parent, its world is equal to its local
                cWorldTf.matrix = cLocalTf.matrix;
                continue;
            }
            else
            {
                auto& pWorldTf = view.get<WorldTransform>(parent);
                cWorldTf.matrix = pWorldTf.matrix * cLocalTf.matrix;
            }

            m_TLasDirty = true;
            cLocalTf.dirty = false;
        }

        if (parent != entt::null)
        {
            // Check if the parent was dirty
            auto& pWorldTf = view.get<WorldTransform>(parent);
            cWorldTf.matrix = pWorldTf.matrix * cLocalTf.matrix;
        }
    }

    RebuildTLas();
}

Entity trayser::Scene::CreateNode()
{
    return CreateNode(m_root);
}

Entity trayser::Scene::CreateNode(Entity parent)
{
    Entity child = m_registry.create();
    AddNode(parent, child);

    return child;
}

Entity trayser::Scene::TraverseModel(const Model& model, int modelNodeIdx, SGNode* parent)
{
    const Model::Node* modelNode = &model.nodes[modelNodeIdx];

    // Create child and components
    Entity child = m_registry.create();
    auto& tf = m_registry.emplace<LocalTransform>(child);
    auto& sgNode = m_registry.emplace<SGNode>(child);
    m_registry.emplace<WorldTransform>(child);

    // If the node has a mesh, add the render component
    if (modelNode->handle != ResourceHandle_Invalid)
    {
        m_registry.emplace<RenderComponent>(child, modelNode->handle);
    }

    // Add new child to parent
    if (parent)
    {
        parent->children.emplace_back(child);
    }

    // Set the local transform
    tf.matrix = modelNode->matrix;
    tf.translation = modelNode->translation;
    tf.orientation = modelNode->orientation;
    tf.scale = modelNode->scale;
    tf.dirty = false;

    // Recursively call children
    for (int childIdx : modelNode->children)
    {
        TraverseModel(model, childIdx, &sgNode);
    }

    return child;
}

Entity trayser::Scene::CreateModel(const Model& model)
{
    return CreateModel(model, m_root);
}

Entity trayser::Scene::CreateModel(const Model& model, Entity parent)
{
    Entity node = entt::null;
    for (auto rootNode : model.rootNodes)
    {
        node = TraverseModel(model, rootNode, &m_registry.get<SGNode>(parent));
    }
    m_sceneGraphDirty = true;
    m_TLasDirty = true;

    return node;
}

void trayser::Scene::AddNode(Entity child)
{
    AddNode(m_root, child);
}

void trayser::Scene::AddNode(Entity parent, Entity child)
{
    if (parent != entt::null)
    {
        auto& parentNode = m_registry.get<SGNode>(parent);
        parentNode.children.emplace_back(child);
    }
    m_registry.emplace<LocalTransform>(child);
    m_registry.emplace<WorldTransform>(child);
    m_registry.emplace<SGNode>(child);

    m_sceneGraphDirty = true;
    m_TLasDirty = true;
}

void trayser::Scene::Clear()
{
    m_registry.clear();
    m_root = CreateNode(entt::null);
}

void trayser::Scene::Rebuild()
{
}

void trayser::Scene::BuildSceneGraph()
{
    auto view = m_registry.view<SGNode>();
    m_traversalBuffer.clear();

    std::stack<std::vector<Entity>> stack;
    stack.push({ m_root });  // start with root path

    while (!stack.empty())
    {
        std::vector<Entity> path = stack.top();
        stack.pop();

        Entity current = path.back();
        const SGNode& node = view.get<SGNode>(current);

        if (node.children.empty())
        {
            // Leaf node: store full path + null
            m_traversalBuffer.insert(m_traversalBuffer.end(), path.begin(), path.end());
            m_traversalBuffer.push_back(entt::null);
        }
        else
        {
            for (Entity child : node.children)
            {
                std::vector<Entity> newPath = path;
                newPath.push_back(child);
                stack.push(newPath);
            }
        }
    }
    m_sceneGraphDirty = false;
}

void trayser::Scene::BuildTLas()
{
    auto toTransformMatrixKHR = [](const glm::mat4& m) {
        VkTransformMatrixKHR t;
        memcpy(&t, glm::value_ptr(glm::transpose(m)), sizeof(t));
        return t;
        };

    // First create the instance data for the TLAS
    std::vector<VkAccelerationStructureInstanceKHR> tlasInstances;
    auto view = g_engine.m_scene.m_registry.view<WorldTransform, RenderComponent>();
    int i = 0;
    for (const auto& [entity, transform, render] : view.each())
    {
        VkAccelerationStructureInstanceKHR asInstance{};
        asInstance.transform = toTransformMatrixKHR(transform.matrix);  // Position of the instance
        asInstance.instanceCustomIndex = render.mesh;                       // gl_InstanceCustomIndexEXT
        asInstance.accelerationStructureReference = g_engine.m_meshPool.Get(render.mesh).BLas.address;
        asInstance.instanceShaderBindingTableRecordOffset = 0;  // We will use the same hit group for all objects
        asInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_CULL_DISABLE_BIT_NV;  // No culling - double sided
        asInstance.mask = 0xFF;
        tlasInstances.emplace_back(asInstance);
        i++;
    }

    Device::Buffer tlasInstancesBuffer;
    {
        // 1) Create destination TLAS instance buffer (device-local, not mapped)
        VkBufferCreateInfo dstInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        dstInfo.size = std::span<VkAccelerationStructureInstanceKHR const>(tlasInstances).size_bytes(); // sizeof(VkAccelerationStructureInstanceKHR) * count
        dstInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        VmaAllocationCreateInfo dstAlloc{};
        dstAlloc.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        //dstAlloc.flags = VMA_ALLOCATION_CREATE_BUFFER_DEVICE_ADDRESS_BIT; // for device address

        VmaAllocationInfo dstAllocInfo;
        vmaCreateBuffer(g_engine.m_device.m_allocator, &dstInfo, &dstAlloc,
            &tlasInstancesBuffer.buffer, &tlasInstancesBuffer.allocation, &dstAllocInfo);

        // 2) Create staging buffer (host-visible, mapped or map manually)
        VkBufferCreateInfo stagingInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        stagingInfo.size = dstInfo.size;
        stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        VmaAllocationCreateInfo stagingAlloc{};
        stagingAlloc.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        stagingAlloc.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT; // optional

        Device::Buffer staging;
        VmaAllocationInfo stagingInfoOut;
        vmaCreateBuffer(g_engine.m_device.m_allocator, &stagingInfo, &stagingAlloc,
            &staging.buffer, &staging.allocation, &stagingInfoOut);

        // 3) Write instance data into staging
        void* mapped = stagingInfoOut.pMappedData;
        if (!mapped) {
            vmaMapMemory(g_engine.m_device.m_allocator, staging.allocation, &mapped);
        }
        memcpy(mapped, tlasInstances.data(), dstInfo.size);
        if (stagingInfoOut.pMappedData == nullptr) {
            vmaUnmapMemory(g_engine.m_device.m_allocator, staging.allocation);
        }

        // 4) Copy staging -> destination
        VkCommandBuffer cmd;
        g_engine.m_device.BeginOneTimeSubmit(cmd);

        VkBufferCopy region{ };
        region.srcOffset = 0;
        region.dstOffset = 0;
        region.size = dstInfo.size;
        vkCmdCopyBuffer(cmd, staging.buffer, tlasInstancesBuffer.buffer, 1, &region);

        g_engine.m_device.EndOneTimeSubmit(); // ensure it waits for completion

        // 5) Destroy staging buffer
        vmaDestroyBuffer(g_engine.m_device.m_allocator, staging.buffer, staging.allocation);

        //EndOneTimeSubmit();
    }
    VkBufferDeviceAddressInfo addrInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
    addrInfo.buffer = tlasInstancesBuffer.buffer;
    VkDeviceAddress tlasInstanceAddr = vkGetBufferDeviceAddress(g_engine.m_device.m_device, &addrInfo);

    // Then create the TLAS geometry
    {
        VkAccelerationStructureGeometryKHR       asGeometry{};
        VkAccelerationStructureBuildRangeInfoKHR asBuildRangeInfo{};

        // Convert the instance information to acceleration structure geometry, similar to primitiveToGeometry()
        VkAccelerationStructureGeometryInstancesDataKHR geometryInstances
        {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
            .pNext = nullptr,
            .arrayOfPointers = VK_FALSE,
            .data = {.deviceAddress = tlasInstanceAddr}
        };
        asGeometry = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
                            .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
                            .geometry = {.instances = geometryInstances} };
        asBuildRangeInfo = { .primitiveCount = static_cast<uint32_t>(i) };

        g_engine.m_device.CreateAccelerationStructure(VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, m_TLas, asGeometry,
            asBuildRangeInfo, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
    }

    vmaDestroyBuffer(g_engine.m_device.m_allocator, tlasInstancesBuffer.buffer, tlasInstancesBuffer.allocation);
}

void trayser::Scene::RebuildTLas()
{
    if (m_TLasDirty)
    {
        if (m_TLasInitialized)
            DestroyTLas();

        BuildTLas();
        m_TLasDirty = false;
        m_TLasInitialized = true;
    }
}

void trayser::Scene::DestroyTLas()
{
    vkDeviceWaitIdle(g_engine.m_device.m_device);

    g_engine.m_device.m_rtFuncs.vkDestroyAccelerationStructureKHR(g_engine.m_device.m_device, m_TLas.accel, nullptr);
    vmaDestroyBuffer(g_engine.m_device.m_allocator, m_TLas.buffer.buffer, m_TLas.buffer.allocation);
}
