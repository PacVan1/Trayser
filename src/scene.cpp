#include <pch.h>

#include <stack>
#include <scene.h>

Scene::Scene() :
    m_dirty(true)
{
    m_root = CreateNode(entt::null);
}

void Scene::Update(float dt)
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
            glm::mat4 T = glm::translate(glm::mat4(1.0f), cLocalTf.position);
            glm::mat4 R = glm::toMat4(cLocalTf.rotation);
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

Entity Scene::CreateNode()
{
    return CreateNode(m_root);
}

Entity Scene::CreateNode(Entity parent)
{
    Entity child = m_registry.create();
    AddNode(parent, child);

    return child;
}

void Scene::CreateMesh(std::shared_ptr<MeshAsset> mesh)
{
    Entity node = CreateNode();
    auto& comp = m_registry.emplace<RenderComponent>(node);
    comp.mesh = mesh; 
}

void Scene::AddNode(Entity child)
{
    AddNode(m_root, child);
}

void Scene::AddNode(Entity parent, Entity child)
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

void Scene::Clear()
{
    m_registry.clear();
    m_root = CreateNode(entt::null);
}

void Scene::Build()
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