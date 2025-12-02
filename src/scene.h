#pragma once

#include <entt/entt.hpp>
#include <types.h>
#include <components.h>
#include <loader.h>
#include <memory>

namespace trayser
{

class Scene
{
public:
            Scene();
    void    Update(float dt);
    Entity  CreateNode();
    Entity  CreateNode(Entity parent);
    Entity  CreateModel(std::shared_ptr<Model> model);
    Entity  CreateModel(std::shared_ptr<Model> model, Entity parent);
    Entity  TraverseModel(std::shared_ptr<Model> model, int modelNodeIdx, SGNode* parent);
    void    AddNode(Entity child);
    void    AddNode(Entity parent, Entity child);
    void    Clear();

    Entity          GetRootEntity() const { return m_root; }
    const SGNode&   GetRootNode() const { return m_registry.get<SGNode>(m_root); }

private:
    void Build();

public:
	entt::registry       m_registry;

private:
    std::vector<Entity>  m_traversalBuffer;
    Entity               m_root;
    bool                 m_dirty;
};

} // namespace trayser