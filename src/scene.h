#pragma once

#include <entt/entt.hpp>
#include <types.h>
#include <components.h>
#include <loader.h>
#include <memory>
#include <device.h>

namespace trayser
{

class Scene
{
public:
    void    Init();
    void    Destroy();
    void    Update(float dt);
    Entity  CreateNode();
    Entity  CreateNode(Entity parent);
    Entity  CreateModel(const Model& model);
    Entity  CreateModel(const Model& model, Entity parent);
    Entity  TraverseModel(const Model& model, int modelNodeIdx, SGNode* parent);
    void    AddNode(Entity child);
    void    AddNode(Entity parent, Entity child);
    void    Clear();

    Entity          GetRootEntity() const { return m_root; }
    const SGNode&   GetRootNode() const { return m_registry.get<SGNode>(m_root); }

private:
    void Rebuild();
    void BuildSceneGraph();
    void BuildTLas();
    void RebuildTLas();
    void DestroyTLas();

public:
	entt::registry       m_registry;
    AccelerationStructure   m_TLas;

private:
    // Scene graph
    std::vector<Entity>  m_traversalBuffer;
    Entity               m_root;
    bool                 m_sceneGraphDirty;

    // Ray tracing
    bool                m_TLasDirty;
    bool                m_TLasInitialized;
};

} // namespace trayser