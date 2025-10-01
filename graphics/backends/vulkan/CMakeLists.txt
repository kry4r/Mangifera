file(GLOB_RECURSE VULKAN_BACKEND_SOURCE
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
)

add_library(vulkan_backend STATIC ${VULKAN_BACKEND_SOURCE})

target_include_directories(vulkan_backend PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${Vulkan_INCLUDE_DIRS})
target_include_directories(vulkan_backend PUBLIC ${PROJECT_SOURCE_DIR}/graphics)
target_include_directories(vulkan_backend PUBLIC ${PROJECT_SOURCE_DIR}/core)


target_link_libraries(vulkan_backend PRIVATE Vulkan::Vulkan spirv-reflect-static glfw)
