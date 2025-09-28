file(GLOB_RECURSE VULKAN_BACKEND_SOURCE
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
)

add_library(vulkan_backend STATIC ${VULKAN_BACKEND_SOURCE})

target_include_directories(vulkan_backend PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(vulkan_backend PRIVATE Vulkan)
