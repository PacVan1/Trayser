#include <pch.h>

#include <vkfuncs.h>

namespace trayser::vkfuncs
{

#define DEFINE_DEVICE_PROC_ADDR(name) \
    PFN_##name name = nullptr

DEFINE_DEVICE_PROC_ADDR(vkCreateAccelerationStructureKHR);
DEFINE_DEVICE_PROC_ADDR(vkCmdBuildAccelerationStructuresKHR);
DEFINE_DEVICE_PROC_ADDR(vkGetAccelerationStructureDeviceAddressKHR);
DEFINE_DEVICE_PROC_ADDR(vkGetAccelerationStructureBuildSizesKHR);
DEFINE_DEVICE_PROC_ADDR(vkCreateRayTracingPipelinesKHR);
DEFINE_DEVICE_PROC_ADDR(vkGetRayTracingShaderGroupHandlesKHR);
DEFINE_DEVICE_PROC_ADDR(vkCmdTraceRaysKHR);
DEFINE_DEVICE_PROC_ADDR(vkDestroyAccelerationStructureKHR);

#undef DEFINE_DEVICE_PROC_ADDR

} // namespace trayser::vkfuncs

void trayser::vkfuncs::Init(VkDevice device)
{
#define LOAD_DEVICE_PROC_ADDR(name) \
        name = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(device, #name))

    LOAD_DEVICE_PROC_ADDR(vkCreateAccelerationStructureKHR);
    LOAD_DEVICE_PROC_ADDR(vkCmdBuildAccelerationStructuresKHR);
    LOAD_DEVICE_PROC_ADDR(vkGetAccelerationStructureDeviceAddressKHR);
    LOAD_DEVICE_PROC_ADDR(vkGetAccelerationStructureBuildSizesKHR);
    LOAD_DEVICE_PROC_ADDR(vkCreateRayTracingPipelinesKHR);
    LOAD_DEVICE_PROC_ADDR(vkGetRayTracingShaderGroupHandlesKHR);
    LOAD_DEVICE_PROC_ADDR(vkCmdTraceRaysKHR);
    LOAD_DEVICE_PROC_ADDR(vkDestroyAccelerationStructureKHR);

#undef LOAD_DEVICE_PROC_ADDR
}