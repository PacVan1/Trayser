#pragma once

// Standard
#include <string>
#include <vector>
#include <filesystem>
#include <unordered_map>
#include <memory>
#include <optional>
#include <span>
#include <array>
#include <functional>
#include <deque>
#include <bitset>
#include <stack>
#include <assert.h>
#include <fstream>
#include <iostream>
#include <numeric>
#include <thread>
#include <chrono>

// Third-party
#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/packing.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <fmt/core.h>
#include <entt/entt.hpp>
#include <magic_enum/magic_enum.hpp>
#include <tiny_gltf.h>
#include <mikktspace.h>
#include <slang/slang.h>
#include <slang/slang-com-ptr.h>
#include <umHalf.h>

// Custom
#include <types.h>
#include <input.h>
#include <vkfuncs.h>
#include <slang_compiler.h>

#define PRINT_ERROR(str) fmt::print("\033[31m[ERROR] {}\033[0m\n", str)
#define PRINT_WARNING(str) fmt::print("\033[33m[WARNING] {}\033[0m\n", str)
#define PRINT_INFO(str) fmt::print("[INFO] {}\n", str)