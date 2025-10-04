file(GLOB_RECURSE MANGO_CORE_SOURCE
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
)

add_library(core STATIC ${MANGO_CORE_SOURCE})

target_include_directories(core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(core PUBLIC ${MANGO_3RDPARTY_DIR})

target_link_libraries(core PUBLIC glm)
