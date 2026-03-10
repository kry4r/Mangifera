add_executable(mangifera_smoke_tests
    smoke/rhi_smoke.cpp
)

target_include_directories(mangifera_smoke_tests PRIVATE ${CMAKE_SOURCE_DIR})

add_test(NAME mangifera_smoke COMMAND mangifera_smoke_tests)

add_executable(mangifera_rhi_tests
    rhi/device_capabilities_tests.cpp
)

target_include_directories(mangifera_rhi_tests PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(mangifera_rhi_tests PRIVATE graphics)

add_test(NAME mangifera_rhi_capabilities COMMAND mangifera_rhi_tests)

add_executable(mangifera_rhi_resource_tests
    rhi/resource_descriptor_tests.cpp
)

target_include_directories(mangifera_rhi_resource_tests PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(mangifera_rhi_resource_tests PRIVATE graphics)

add_test(NAME resource_descriptor COMMAND mangifera_rhi_resource_tests)

add_executable(mangifera_render_core_tests
    render_core/frame_context_tests.cpp
)

target_include_directories(mangifera_render_core_tests PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(mangifera_render_core_tests PRIVATE app)

add_test(NAME frame_context COMMAND mangifera_render_core_tests)

add_executable(mangifera_render_scene_tests
    render_core/render_scene_extractor_tests.cpp
)

target_include_directories(mangifera_render_scene_tests PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(mangifera_render_scene_tests PRIVATE app core)

add_test(NAME render_scene_extractor COMMAND mangifera_render_scene_tests)

add_executable(mangifera_render_graph_tests
    render_core/render_graph_tests.cpp
)

target_include_directories(mangifera_render_graph_tests PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(mangifera_render_graph_tests PRIVATE app)

add_test(NAME render_graph COMMAND mangifera_render_graph_tests)

add_executable(mangifera_render_targets_tests
    render_core/render_targets_tests.cpp
)

target_include_directories(mangifera_render_targets_tests PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(mangifera_render_targets_tests PRIVATE app)

add_test(NAME render_targets COMMAND mangifera_render_targets_tests)

add_executable(mangifera_sensor_pass_tests
    render_core/sensor_pass_tests.cpp
)

target_include_directories(mangifera_sensor_pass_tests PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(mangifera_sensor_pass_tests PRIVATE app)

add_test(NAME sensor_pass COMMAND mangifera_sensor_pass_tests)

add_executable(mangifera_headless_tests
    headless/headless_runner_tests.cpp
)

target_include_directories(mangifera_headless_tests PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(mangifera_headless_tests PRIVATE app)

add_test(NAME headless_runner COMMAND mangifera_headless_tests)

add_executable(mangifera_raytracing_capability_tests
    rhi/raytracing_capability_tests.cpp
)

target_include_directories(mangifera_raytracing_capability_tests PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(mangifera_raytracing_capability_tests PRIVATE app graphics)

add_test(NAME raytracing_capability COMMAND mangifera_raytracing_capability_tests)
