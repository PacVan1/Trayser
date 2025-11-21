
# Add source to this project's executable.
add_executable (engine 
  main.cpp
  types.h
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

target_link_libraries(engine PUBLIC vma glm Vulkan::Vulkan fmt::fmt stb_image SDL2::SDL2 vkbootstrap imgui fastgltf::fastgltf)

# Warnings as errors
#if (MSVC)
#    target_compile_options(engine PRIVATE /WX)
#else() # GCC/Clang
#    target_compile_options(engine PRIVATE -Werror)
#endif()

if(WIN32)
  add_custom_command(TARGET engine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:engine> $<TARGET_FILE_DIR:engine>
    COMMAND_EXPAND_LISTS
    )
endif()