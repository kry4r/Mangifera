cmake_minimum_required(VERSION 3.25)
project(Mangifera)

set(CMAKE_CXX_STANDARD 17)
find_package(Vulkan REQUIRED)

include_directories(core)

file(GLOB CORE_SOURCES "core/**/*.cpp")

add_executable(Mangifera Mangifera.cpp ${CORE_SOURCES})
