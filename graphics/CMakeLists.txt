file(GLOB_RECURSE GRAPHICS_SOURCE
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
)

add_library(graphics STATIC ${GRAPHICS_SOURCE})
add_subdirectory(backends/vulkan)

target_include_directories(graphics PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(graphics PRIVATE shaderc vulkan_backend)
