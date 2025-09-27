cmake_minimum_required(VERSION 3.25)
project(Mangifera)

set(CMAKE_CXX_STANDARD 17)
find_package(Vulkan REQUIRED)

add_subdirectory(3rdparty/glm)
add_subdirectory(3rdparty/imgui)

include_directories(3rdparty/glm)
include_directories(3rdparty/imgui)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB CORE_SOURCES "core/**/*.cpp")

add_executable(Mangifera Mangifera.cpp ${CORE_SOURCES})
