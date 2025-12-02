
# Add source to this project's executable.
add_executable (engine 
  main.cpp
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

# Find slangc
find_program(SLANG_COMPILER slangc
    HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/
)

# Collect Slang shaders (stage-specific suffixes)
file(GLOB_RECURSE SLANG_VERTEX_SHADERS "${PROJECT_SOURCE_DIR}/shaders/*.vert.slang")
file(GLOB_RECURSE SLANG_FRAGMENT_SHADERS "${PROJECT_SOURCE_DIR}/shaders/*.frag.slang")
file(GLOB_RECURSE SLANG_COMPUTE_SHADERS "${PROJECT_SOURCE_DIR}/shaders/*.comp.slang")

# Add them to the project but not to compilation
set_source_files_properties(${SLANG_VERTEX_SHADERS} ${SLANG_FRAGMENT_SHADERS} ${SLANG_COMPUTE_SHADERS} PROPERTIES HEADER_FILE_ONLY TRUE)

# Attach them to your target (important!)
target_sources(engine PRIVATE
    ${SLANG_VERTEX_SHADERS}
    ${SLANG_FRAGMENT_SHADERS}
    ${SLANG_COMPUTE_SHADERS}
)

# Organize them in a filter called "Shaders"
source_group("Shaders" FILES ${SLANG_VERTEX_SHADERS} ${SLANG_FRAGMENT_SHADERS} ${SLANG_COMPUTE_SHADERS})

# Attach post-build commands to 'engine'
add_custom_command(TARGET engine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Compiling Slang shaders to SPIR-V..."
)

# Slang vertex loop
foreach(SLANG ${SLANG_VERTEX_SHADERS})
    get_filename_component(FILE_NAME ${SLANG} NAME_WE)
    set(SPIRV "${CMAKE_BINARY_DIR}/shaders/${FILE_NAME}.spv")

    add_custom_command(TARGET engine POST_BUILD
        COMMAND ${SLANG_COMPILER} ${SLANG} -profile vs_5_0 -target spirv -o ${SPIRV}
        COMMENT "Compiling Slang vertex ${FILE_NAME}"
    )
endforeach()

# Slang fragment loop
foreach(SLANG ${SLANG_FRAGMENT_SHADERS})
    get_filename_component(FILE_NAME ${SLANG} NAME_WE)
    set(SPIRV "${CMAKE_BINARY_DIR}/shaders/${FILE_NAME}.spv")

    add_custom_command(TARGET engine POST_BUILD
        COMMAND ${SLANG_COMPILER} ${SLANG} -profile ps_5_0 -target spirv -o ${SPIRV}
        COMMENT "Compiling Slang fragment ${FILE_NAME}"
    )
endforeach()

# Slang compute loop
foreach(SLANG ${SLANG_COMPUTE_SHADERS})
    get_filename_component(FILE_NAME ${SLANG} NAME_WE)
    set(SPIRV "${CMAKE_BINARY_DIR}/shaders/${FILE_NAME}.spv")

    add_custom_command(TARGET engine POST_BUILD
        COMMAND ${SLANG_COMPILER} ${SLANG} -profile cs_5_0 -target spirv -o ${SPIRV}
        COMMENT "Compiling Slang compute ${FILE_NAME}"
    )
endforeach()