#include <pch.h>

#include <stack>
#include <scene.h>
#include <engine.h>

trayser::Scene::Scene() :
    m_dirty(true)
{
    m_root = CreateNode(entt::null);
}

void trayser::Scene::Update(float dt)
{
    if (m_dirty)
        Build();

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
        }

        if (parent != entt::null)
        {
            // Check if the parent was dirty
            auto& pWorldTf = view.get<WorldTransform>(parent);
            cWorldTf.matrix = pWorldTf.matrix * cLocalTf.matrix;
        }
    }
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
    m_dirty = true;

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

    m_dirty = true;
}

void trayser::Scene::Clear()
{
    m_registry.clear();
    m_root = CreateNode(entt::null);
}

void trayser::Scene::Build()
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
    m_dirty = false;
}