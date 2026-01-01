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
    float4x4 matrix = float4x4(1.0f);

    float3 GetPosition()
    {
        return float3(matrix[3]);
    }
    // Only works with uniform scaling
    float GetScale()
    {
        float4x4& m = matrix;
        float s = glm::length(float3(m[0][0], m[1][0], m[2][0]));
        return s;
    }
};

// Empty struct for finding all transforms that use a gpu::SphereLight
struct SphereLight {};

struct RenderComponent
{
    //std::shared_ptr<Mesh> mesh;
    MeshHandle mesh;
};

} // namespace components

using namespace components;

} // namespace trayser