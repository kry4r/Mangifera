cmake_minimum_required(VERSION 3.16)
project(Mangifera)

set(CMAKE_CXX_STANDARD 20)
find_package(Vulkan REQUIRED)

message(STATUS "Vulkan include: ${Vulkan_INCLUDE_DIRS}")
message(STATUS "Vulkan library: ${Vulkan_LIBRARIES}")

add_subdirectory(3rdparty/glm)
add_subdirectory(3rdparty/imgui)
add_subdirectory(3rdparty/glfw)

set(MANGO_3RDPARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty)

# Define a variable for third party includes
set(SPIRV_REFLECT_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/3rdparty/spirv-reflect")

# Configure SPIRV-Reflect
set(SPIRV_REFLECT_EXECUTABLE ON CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_STATIC_LIB ON CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_BUILD_TESTS OFF CACHE BOOL "" FORCE)

add_subdirectory(3rdparty/spirv-reflect)

set(SHADERC_SKIP_TESTS ON CACHE BOOL "Skip building tests" FORCE)
set(SHADERC_SKIP_EXAMPLES ON CACHE BOOL "Skip building examples" FORCE)
set(SHADERC_SKIP_INSTALL ON CACHE BOOL "Skip shaderc install" FORCE)
set(CMAKE_SKIP_INSTALL_RULES ON CACHE BOOL "Skip all install() rules" FORCE)

if(WIN32)
    add_definitions(-DVK_USE_PLATFORM_WIN32_KHR)
elseif(UNIX AND NOT APPLE)
    add_definitions(-DVK_USE_PLATFORM_XCB_KHR)
    # add_definitions(-DVK_USE_PLATFORM_XLIB_KHR)
elseif(APPLE)
    add_definitions(-DVK_USE_PLATFORM_METAL_EXT)
endif()

add_subdirectory(3rdparty/shaderc)

add_subdirectory(core)
add_subdirectory(graphics)
add_subdirectory(app)

#file(GLOB CORE_SOURCES "core/**/*.cpp")

add_executable(Mangifera Mangifera.cpp)

target_link_libraries(Mangifera PUBLIC core app graphics)
