cmake_minimum_required(VERSION 3.25)
project(Mangifera)

set(CMAKE_CXX_STANDARD 17)
find_package(Vulkan REQUIRED)

message(STATUS "Vulkan include: ${Vulkan_INCLUDE_DIRS}")
message(STATUS "Vulkan library: ${Vulkan_LIBRARIES}")

add_subdirectory(core)
add_subdirectory(3rdparty/glm)
add_subdirectory(3rdparty/imgui)
add_subdirectory(3rdparty/glfw)

set(SHADERC_SKIP_TESTS ON CACHE BOOL "Skip building tests" FORCE)
set(SHADERC_SKIP_EXAMPLES ON CACHE BOOL "Skip building examples" FORCE)
set(SHADERC_SKIP_INSTALL ON CACHE BOOL "Skip install" FORCE)
add_subdirectory(3rdparty/shaderc)

#file(GLOB CORE_SOURCES "core/**/*.cpp")

add_executable(Mangifera Mangifera.cpp core)

target_link_libraries(Mangifera PRIVATE core glm imgui ${Vulkan_LIBRARIES} glfw)
