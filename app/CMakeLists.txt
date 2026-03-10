file(GLOB_RECURSE APP_SOURCE
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
)

list(APPEND APP_SOURCE
    ${CMAKE_CURRENT_SOURCE_DIR}/headless/headless_runner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/headless/headless_runner.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/render_features/passes/depth_prepass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/render_features/passes/depth_prepass.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/render_features/passes/sensor_export_pass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/render_features/passes/sensor_export_pass.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/render_features/passes/post/bloom_pass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/render_features/passes/post/bloom_pass.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/render_features/passes/post/tonemap_pass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/render_features/passes/post/tonemap_pass.hpp
)
list(REMOVE_DUPLICATES APP_SOURCE)

add_library(app STATIC ${APP_SOURCE})

target_include_directories(app
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/3rdparty
        ${CMAKE_SOURCE_DIR}/3rdparty/tinyexr
)
target_link_libraries(app PUBLIC
    core
    glfw
    graphics
    imgui
    glm
)
