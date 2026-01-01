#pragma once

#include <vulkan/vulkan.h>

namespace trayser::vkfuncs
{

#define DECLARE_DEVICE_PROC_ADDR(name) \
    extern PFN_##name name

DECLARE_DEVICE_PROC_ADDR(vkCreateAccelerationStructureKHR);
DECLARE_DEVICE_PROC_ADDR(vkCmdBuildAccelerationStructuresKHR);
DECLARE_DEVICE_PROC_ADDR(vkGetAccelerationStructureDeviceAddressKHR);
DECLARE_DEVICE_PROC_ADDR(vkGetAccelerationStructureBuildSizesKHR);
DECLARE_DEVICE_PROC_ADDR(vkCreateRayTracingPipelinesKHR);
DECLARE_DEVICE_PROC_ADDR(vkGetRayTracingShaderGroupHandlesKHR);
DECLARE_DEVICE_PROC_ADDR(vkCmdTraceRaysKHR);
DECLARE_DEVICE_PROC_ADDR(vkDestroyAccelerationStructureKHR);

#undef DECLARE_DEVICE_PROC_ADDR

void Init(VkDevice device);

} // namespace trayser::vkfuncs