add_library(imgui STATIC
    ${CMAKE_SOURCE_DIR}/3rdparty/imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/3rdparty/imgui/imgui_demo.cpp
    ${CMAKE_SOURCE_DIR}/3rdparty/imgui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/3rdparty/imgui/imgui_tables.cpp
    ${CMAKE_SOURCE_DIR}/3rdparty/imgui/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/3rdparty/imgui/backends/imgui_impl_glfw.cpp
    ${CMAKE_SOURCE_DIR}/3rdparty/imgui/backends/imgui_impl_vulkan.cpp
)

target_include_directories(imgui
    PUBLIC
        ${CMAKE_SOURCE_DIR}/3rdparty/imgui
        ${CMAKE_SOURCE_DIR}/3rdparty/imgui/backends
        ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(imgui
    PUBLIC
        glfw
        ${Vulkan_LIBRARIES}
)

if(MSVC)
    target_compile_definitions(imgui PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
