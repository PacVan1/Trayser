file(GLOB_RECURSE HEADER_FILES
    "${PROJECT_SOURCE_DIR}/src/*.h"
    "${PROJECT_SOURCE_DIR}/src/*.hpp"
    "${PROJECT_SOURCE_DIR}/src/*.inl"
    )

file(GLOB_RECURSE SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/src/*.cc"
    "${PROJECT_SOURCE_DIR}/src/*.cpp"
)

# Add source to this project's executable.
add_executable (Trayser 
    ${HEADER_FILES}
    ${SOURCE_FILES}
)

target_sources(Trayser PRIVATE
    ${PROJECT_SOURCE_DIR}/src/umHalf/umHalf.h
    ${PROJECT_SOURCE_DIR}/src/umHalf/umHalf.inl
)

target_include_directories(Trayser PRIVATE 
    ${PROJECT_SOURCE_DIR}/src/umHalf)

set_property(TARGET Trayser PROPERTY CXX_STANDARD 20)
target_compile_definitions(Trayser PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)
target_include_directories(Trayser PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_precompile_headers(Trayser PRIVATE pch.h)

target_link_libraries(Trayser PUBLIC 
  vma 
  glm 
  Vulkan::Vulkan 
  fmt::fmt 
  stb_image 
  SDL2::SDL2 
  imgui 
  fastgltf::fastgltf 
  entt 
  magic_enum 
  tinygltf)

# Warnings as errors
#if (MSVC)
#    target_compile_options(Trayser PRIVATE /WX)
#else() # GCC/Clang
#    target_compile_options(Trayser PRIVATE -Werror)
#endif()

# Enable multi-threaded compilation
if (MSVC)
    add_compile_options(/MP)
endif()

if(WIN32)
  add_custom_command(TARGET Trayser POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:Trayser> $<TARGET_FILE_DIR:Trayser>
    COMMAND_EXPAND_LISTS
    )
endif()

set(SLANG_INCLUDE_DIR "$ENV{VULKAN_SDK}/Include")
set(SLANG_LIB_DIR "$ENV{VULKAN_SDK}/Lib")
set(SLANG_BIN_DIR "$ENV{VULKAN_SDK}/Bin")
target_include_directories(Trayser PRIVATE ${SLANG_INCLUDE_DIR})
target_link_libraries(Trayser PRIVATE
    $<$<CONFIG:Debug>:${SLANG_LIB_DIR}/slangd.lib> # Windows (.lib)
    $<$<CONFIG:Release>:${SLANG_LIB_DIR}/slang.lib>
)

add_custom_command(TARGET Trayser POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${SLANG_BIN_DIR}/slang.dll"
    $<TARGET_FILE_DIR:Trayser>)

# Make sure the shaders output directory exists
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/shaders)

file(GLOB_RECURSE SLANG_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/shaders/*.slang"
    "${PROJECT_SOURCE_DIR}/shaders/*.h"
    )

# Add them to the project but not to compilation
set_source_files_properties(${SLANG_SOURCE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)

# Attach them to your target (important!)
target_sources(Trayser PRIVATE
    ${SLANG_SOURCE_FILES}
)

# Organize them in a filter called "Shaders"
source_group("Shaders" FILES ${SLANG_SOURCE_FILES})