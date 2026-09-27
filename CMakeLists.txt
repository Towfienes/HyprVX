cmake_minimum_required(VERSION 3.25)

project(
    HyprVX
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(HYPRVX_BUILD_TESTS "Build HyprVX tests" ON)

add_library(hyprvx_core STATIC
    core/src/Camera.cpp
    core/src/CameraMotion.cpp
    core/src/Island.cpp
    core/src/IslandInteraction.cpp
    core/src/SpatialNavigation.cpp
    core/src/ViewportFraming.cpp
    core/src/NavigationState.cpp
    core/src/OutputState.cpp
    core/src/OutputTopology.cpp
    core/src/World.cpp
)

target_include_directories(
    hyprvx_core
    PUBLIC
        core/include
)

add_library(hyprvx_hyprland_ipc STATIC
    daemon/src/EventParser.cpp
    daemon/src/SemanticEvent.cpp
    daemon/src/SemanticEventAdapter.cpp
    daemon/src/RuntimeMirror.cpp
    daemon/src/RuntimeBridge.cpp
    daemon/src/Socket2Stream.cpp
)

target_include_directories(
    hyprvx_hyprland_ipc
    PUBLIC
        daemon/include
)

target_compile_features(
    hyprvx_hyprland_ipc
    PUBLIC
        cxx_std_20
)

target_compile_options(
    hyprvx_hyprland_ipc
    PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wshadow
        >
)

target_compile_features(
    hyprvx_core
    PUBLIC
        cxx_std_20
)

target_compile_options(
    hyprvx_core
    PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wshadow
        >
)

add_executable(
    hyprvx_simulator
    simulator/main.cpp
)

target_link_libraries(
    hyprvx_simulator
    PRIVATE
        hyprvx_core
)

set_target_properties(
    hyprvx_simulator
    PROPERTIES
        OUTPUT_NAME hyprvx-sim
)


find_package(PkgConfig REQUIRED)

pkg_check_modules(
    SDL3
    REQUIRED
    IMPORTED_TARGET
    sdl3
)

add_executable(
    hyprvx_visual_simulator
    simulator/visual_main.cpp
)

target_link_libraries(
    hyprvx_visual_simulator
    PRIVATE
        hyprvx_core
        PkgConfig::SDL3
)

set_target_properties(
    hyprvx_visual_simulator
    PROPERTIES
        OUTPUT_NAME hyprvx-visual-sim
)


add_executable(
    hyprvx_multi_output_simulator
    simulator/multi_output_main.cpp
)

target_link_libraries(
    hyprvx_multi_output_simulator
    PRIVATE
        hyprvx_core
        PkgConfig::SDL3
)

set_target_properties(
    hyprvx_multi_output_simulator
    PROPERTIES
        OUTPUT_NAME hyprvx-multi-output-sim
)


add_executable(
    hyprvx_topology_simulator
    simulator/topology_main.cpp
)

target_link_libraries(
    hyprvx_topology_simulator
    PRIVATE
        hyprvx_core
        PkgConfig::SDL3
)

set_target_properties(
    hyprvx_topology_simulator
    PROPERTIES
        OUTPUT_NAME hyprvx-topology-sim
)

add_executable(
    hyprvx_ipc_watch
    daemon/src/ipc_watch_main.cpp
)

target_link_libraries(
    hyprvx_ipc_watch
    PRIVATE
        hyprvx_hyprland_ipc
)

set_target_properties(
    hyprvx_ipc_watch
    PROPERTIES
        OUTPUT_NAME hyprvx-ipc-watch
)

include(CTest)

if(BUILD_TESTING AND HYPRVX_BUILD_TESTS)
    find_package(GTest REQUIRED)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    add_executable(
        hyprvx_core_tests

        tests/unit/test_geometry.cpp
        tests/unit/test_camera.cpp
        tests/unit/test_camera_motion.cpp
        tests/unit/test_island.cpp
        tests/unit/test_island_interaction.cpp
        tests/unit/test_spatial_navigation.cpp
        tests/unit/test_viewport_framing.cpp
        tests/unit/test_navigation_state.cpp
        tests/unit/test_output_state.cpp
        tests/unit/test_output_topology.cpp
        tests/unit/test_world.cpp
        tests/unit/test_world_ownership.cpp
    )

    target_link_libraries(
        hyprvx_core_tests
        PRIVATE
            hyprvx_core
            GTest::gtest_main
    )

    include(GoogleTest)

    gtest_discover_tests(
        hyprvx_core_tests
    )

    add_executable(
        hyprvx_hyprland_ipc_tests
        tests/unit/test_hyprland_event_parser.cpp
        tests/unit/test_semantic_event_adapter.cpp
        tests/unit/test_runtime_mirror.cpp
        tests/unit/test_socket2_stream.cpp
        tests/unit/test_runtime_bridge.cpp
    )

    target_link_libraries(
        hyprvx_hyprland_ipc_tests
        PRIVATE
            hyprvx_hyprland_ipc
            GTest::gtest_main
    )

    gtest_discover_tests(
        hyprvx_hyprland_ipc_tests
    )

    add_test(
        NAME HyprlandIpcWatchReplay
        COMMAND
            ${CMAKE_COMMAND}
            -DWATCH_EXECUTABLE=$<TARGET_FILE:hyprvx_ipc_watch>
            -DCAPTURE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/data/runtime_bridge_capture.txt
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_ipc_watch_replay.cmake
    )

    foreach(test_case IN ITEMS
        SocketDiscoveryTest.test_runtime_socket_is_first_candidate
        SocketDiscoveryTest.test_tmp_socket_is_fallback_candidate
        SocketDiscoveryTest.test_duplicate_runtime_and_fallback_path_is_listed_once
        SocketDiscoveryTest.test_missing_signature_raises_clear_error
        SocketDiscoveryTest.test_find_socket_returns_first_existing_candidate
        SocketDiscoveryTest.test_find_socket_error_lists_candidates
        EventLineBufferTest.test_fragmented_event_waits_for_newline
        EventLineBufferTest.test_multiple_events_in_single_chunk
        EventLineBufferTest.test_empty_lines_are_preserved
        EventLineBufferTest.test_crlf_delimiter_is_normalized
        EventLineBufferTest.test_finish_returns_trailing_partial_event
        EventLineBufferTest.test_finish_is_empty_after_complete_event
        EventLineBufferTest.test_invalid_utf8_is_replaced
    )
        string(REPLACE "." "_" test_name "${test_case}")
        add_test(
            NAME "HyprlandEventProbe.${test_name}"
            COMMAND
                ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/test_hyprland_event_probe.py
                ${test_case}
        )
    endforeach()
endif()
