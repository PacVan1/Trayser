#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <types.h>
#include <loader.h>
#include <memory>

namespace trayser
{

namespace components
{

struct SGNode
{
	std::vector<Entity> children;
};

struct LocalTransform
{
    glm::vec3 translation = glm::vec3(0.0f);
    glm::quat orientation = glm::quat();
    glm::vec3 scale       = glm::vec3(1.0f);
    glm::mat4 matrix      = glm::mat4(1.0f);
    bool dirty = true;
};

struct WorldTransform
{
    glm::mat4 matrix = glm::mat4(1.0f);
};

struct RenderComponent
{
    std::shared_ptr<Mesh> mesh;
};

} // namespace components

using namespace components;

} // namespace trayser