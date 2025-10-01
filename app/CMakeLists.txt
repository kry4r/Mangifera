file(GLOB_RECURSE APP_SOURCE
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
)

add_library(app STATIC ${APP_SOURCE})

target_include_directories(app
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(app PUBLIC
    glfw
    graphics
)
