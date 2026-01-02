cmake_minimum_required (VERSION 3.8)
project ("Trayser")

# Add the executable ########################################
file(GLOB_RECURSE HEADER_FILES
    "${PROJECT_SOURCE_DIR}/source/*.h"
    "${PROJECT_SOURCE_DIR}/source/*.hpp"
    "${PROJECT_SOURCE_DIR}/source/*.inl"
    )

file(GLOB_RECURSE SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/source/*.cc"
    "${PROJECT_SOURCE_DIR}/source/*.cpp"
)

add_executable (Trayser 
    ${HEADER_FILES}
    ${SOURCE_FILES}
)

# Add external sources ########################################
file(GLOB_RECURSE IMGUI_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/external/imgui/*.cpp"
)

target_sources(Trayser PRIVATE
    ${PROJECT_SOURCE_DIR}/external/mikktspace/mikktspace.cpp
    ${PROJECT_SOURCE_DIR}/external/tinygltf/tiny_gltf.cc
    ${IMGUI_SOURCE_FILES}
)

# Include directories ########################################
target_include_directories(Trayser PRIVATE ${PROJECT_SOURCE_DIR}/external/umHalf)
target_include_directories(Trayser PRIVATE ${PROJECT_SOURCE_DIR}/external/entt/src)
target_include_directories(Trayser PRIVATE ${PROJECT_SOURCE_DIR}/external/magic_enum/include)
target_include_directories(Trayser PRIVATE ${PROJECT_SOURCE_DIR}/external/tinygltf)
target_include_directories(Trayser PRIVATE ${PROJECT_SOURCE_DIR}/external/mikktspace)
target_include_directories(Trayser PRIVATE ${PROJECT_SOURCE_DIR}/external/vma)
target_include_directories(Trayser PRIVATE ${PROJECT_SOURCE_DIR}/external/glm)
target_include_directories(Trayser PRIVATE ${PROJECT_SOURCE_DIR}/external/stb_image)
target_include_directories(Trayser PRIVATE ${PROJECT_SOURCE_DIR}/external/imgui)
target_include_directories(Trayser PRIVATE ${PROJECT_SOURCE_DIR}/shaders)

# Libraries ##################################################
find_package(Vulkan REQUIRED)
add_subdirectory(${PROJECT_SOURCE_DIR}/external/fmt EXCLUDE_FROM_ALL)
add_subdirectory(${PROJECT_SOURCE_DIR}/external/SDL EXCLUDE_FROM_ALL)

target_link_libraries(Trayser PUBLIC 
  Vulkan::Vulkan 
  fmt::fmt 
  SDL2::SDL2)

# Misc #######################################################
set_property(TARGET Trayser PROPERTY CXX_STANDARD 20)
target_compile_definitions(Trayser PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)
target_include_directories(Trayser PUBLIC ${PROJECT_SOURCE_DIR}/source)
target_precompile_headers(Trayser PRIVATE ${PROJECT_SOURCE_DIR}/source/pch.h)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")

# Enable multi-processor compilation for MSVC
if (MSVC) 
add_compile_options(/MP) 
endif() 

# Warnings as errors
#if (MSVC)
#    target_compile_options(Trayser PRIVATE /WX)
#else() # GCC/Clang
#    target_compile_options(Trayser PRIVATE -Werror)
#endif()

if(WIN32)
  add_custom_command(TARGET Trayser POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:Trayser> $<TARGET_FILE_DIR:Trayser>
    COMMAND_EXPAND_LISTS
    )
endif()

# Project filters ############################################
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
    "${PROJECT_SOURCE_DIR}/shaders/*.hpp"
    )

# Add them to the project but not to compilation
set_source_files_properties(${SLANG_SOURCE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)

# Attach them to your target (important!)
target_sources(Trayser PRIVATE
    ${SLANG_SOURCE_FILES}
)

# Organize them in a filter called "Shaders"
source_group("Shaders" FILES ${SLANG_SOURCE_FILES})