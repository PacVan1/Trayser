
# Add source to this project's executable.
add_executable (engine 
  main.cpp
  gbuffer.h
  gbuffer.cpp
  staging.h
  staging.cpp
  ray_tracing.h
  ray_tracing.cpp
  device.h
  device.cpp
  mikktspace.h
  mikktspace.cpp
  tinygltf/tiny_gltf.cc
  vk_mem_alloc.cpp
  types.h
  resources.h
  resources.cpp
  scene.h
  scene.cpp
  components.h
  components.cpp
  input.h
  input.cpp
  util.h
  util.cpp
  editor.h
  editor.cpp
  pch.h
  initializers.cpp
  initializers.h
  images.h
  images.cpp 
  descriptors.h
  descriptors.cpp
  pipelines.h
  pipelines.cpp
  engine.h
  engine.cpp
  loader.h
  loader.cpp
  camera.cpp
  camera.h
)

set_property(TARGET engine PROPERTY CXX_STANDARD 20)
target_compile_definitions(engine PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)
target_include_directories(engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_precompile_headers(engine PRIVATE pch.h)

target_link_libraries(engine PUBLIC 
  vma 
  glm 
  Vulkan::Vulkan 
  fmt::fmt 
  stb_image 
  SDL2::SDL2 
  vkbootstrap 
  imgui 
  fastgltf::fastgltf 
  entt 
  magic_enum 
  tinygltf)

# Warnings as errors
#if (MSVC)
#    target_compile_options(engine PRIVATE /WX)
#else() # GCC/Clang
#    target_compile_options(engine PRIVATE -Werror)
#endif()

# Enable multi-threaded compilation
if (MSVC)
    add_compile_options(/MP)
endif()

if(WIN32)
  add_custom_command(TARGET engine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:engine> $<TARGET_FILE_DIR:engine>
    COMMAND_EXPAND_LISTS
    )
endif()

set(SLANG_INCLUDE_DIR "$ENV{VULKAN_SDK}/Include")
set(SLANG_LIB_DIR "$ENV{VULKAN_SDK}/Lib")
set(SLANG_BIN_DIR "$ENV{VULKAN_SDK}/Bin")
target_include_directories(engine PRIVATE ${SLANG_INCLUDE_DIR})
target_link_libraries(engine PRIVATE
    $<$<CONFIG:Debug>:${SLANG_LIB_DIR}/slangd.lib> # Windows (.lib)
    $<$<CONFIG:Release>:${SLANG_LIB_DIR}/slang.lib>
)

add_custom_command(TARGET engine POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${SLANG_BIN_DIR}/slang.dll"
    $<TARGET_FILE_DIR:engine>)

# Make sure the shaders output directory exists
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/shaders)

file(GLOB_RECURSE SLANG_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/shaders/*.slang"
    )

# Add them to the project but not to compilation
set_source_files_properties(${SLANG_SOURCE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)

# Attach them to your target (important!)
target_sources(engine PRIVATE
    ${SLANG_SOURCE_FILES}
)

# Organize them in a filter called "Shaders"
source_group("Shaders" FILES ${SLANG_SOURCE_FILES})