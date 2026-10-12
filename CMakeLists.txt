cmake_minimum_required(VERSION 3.25)

project(
    HyprVX
    VERSION 0.1.0
    LANGUAGES C CXX
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(HYPRVX_BUILD_TESTS "Build HyprVX tests" ON)
option(HYPRVX_WITH_SDL "Build SDL simulators" OFF)
option(HYPRVX_WITH_WAYLAND_PLATFORM "Build the Wayland platform" OFF)
option(HYPRVX_WITH_HYPRLAND_PLUGIN "Build the Hyprland plugin" OFF)

add_library(hyprvx_core STATIC
    core/src/Camera.cpp
    core/src/CameraMotion.cpp
    core/src/DriftModel.cpp
    core/src/GestureModel.cpp
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

add_library(hyprvx_runtime_projection STATIC
    daemon/src/SpatialProjection.cpp
    daemon/src/ProjectionStateStore.cpp
)

add_library(hyprvx_runtime_navigation STATIC
    daemon/src/RuntimeCameraController.cpp
    daemon/src/RuntimeNavigationInput.cpp
)
target_include_directories(hyprvx_runtime_navigation PUBLIC daemon/include)
target_link_libraries(hyprvx_runtime_navigation PUBLIC hyprvx_runtime_projection hyprvx_core)
target_compile_features(hyprvx_runtime_navigation PUBLIC cxx_std_20)
target_compile_options(hyprvx_runtime_navigation PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Wconversion -Wshadow>
)

find_package(nlohmann_json 3.2.0 REQUIRED)

add_library(hyprvx_nested_guard STATIC platform/src/NestedGuard.cpp)
set_target_properties(hyprvx_nested_guard PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(hyprvx_nested_guard PUBLIC platform/include)
target_link_libraries(hyprvx_nested_guard PUBLIC nlohmann_json::nlohmann_json)
target_compile_features(hyprvx_nested_guard PUBLIC cxx_std_20)

add_executable(hyprvx_nested_check platform/src/nested_check_main.cpp)
set_target_properties(hyprvx_nested_check PROPERTIES OUTPUT_NAME hyprvx-nested-check)
target_link_libraries(hyprvx_nested_check PRIVATE hyprvx_nested_guard)

target_include_directories(
    hyprvx_runtime_projection
    PUBLIC
        daemon/include
)

target_link_libraries(
    hyprvx_runtime_projection
    PUBLIC
        hyprvx_core
        hyprvx_hyprland_ipc
        nlohmann_json::nlohmann_json
)

target_compile_features(
    hyprvx_runtime_projection
    PUBLIC
        cxx_std_20
)

target_compile_options(
    hyprvx_runtime_projection
    PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wshadow
        >
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


if(HYPRVX_WITH_SDL OR HYPRVX_WITH_WAYLAND_PLATFORM OR HYPRVX_WITH_HYPRLAND_PLUGIN)
    find_package(PkgConfig REQUIRED)
endif()

if(HYPRVX_WITH_WAYLAND_PLATFORM)
    pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)
    pkg_check_modules(XKBCOMMON REQUIRED IMPORTED_TARGET xkbcommon)
    pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED wayland-protocols)
    pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
    find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)
    set(HYPRVX_PROTOCOL_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated-protocols)
    file(MAKE_DIRECTORY ${HYPRVX_PROTOCOL_DIR})

    function(hyprvx_client_protocol protocol_name xml_path)
        add_custom_command(
            OUTPUT ${HYPRVX_PROTOCOL_DIR}/${protocol_name}-client-protocol.h
                   ${HYPRVX_PROTOCOL_DIR}/${protocol_name}-protocol.c
            COMMAND ${WAYLAND_SCANNER} client-header ${xml_path}
                    ${HYPRVX_PROTOCOL_DIR}/${protocol_name}-client-protocol.h
            COMMAND ${WAYLAND_SCANNER} private-code ${xml_path}
                    ${HYPRVX_PROTOCOL_DIR}/${protocol_name}-protocol.c
            DEPENDS ${xml_path}
            VERBATIM)
        set(HYPRVX_GENERATED_PROTOCOLS ${HYPRVX_GENERATED_PROTOCOLS}
            ${HYPRVX_PROTOCOL_DIR}/${protocol_name}-protocol.c
            ${HYPRVX_PROTOCOL_DIR}/${protocol_name}-client-protocol.h PARENT_SCOPE)
    endfunction()

    hyprvx_client_protocol(wlr-layer-shell ${CMAKE_CURRENT_SOURCE_DIR}/platform/protocols/wlr-layer-shell-unstable-v1.xml)
    hyprvx_client_protocol(pointer-gestures ${WAYLAND_PROTOCOLS_DIR}/unstable/pointer-gestures/pointer-gestures-unstable-v1.xml)
    hyprvx_client_protocol(xdg-shell ${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml)

    add_library(hyprvx_wayland_platform STATIC
        platform/src/NativeSession.cpp
        platform/src/NativeEventBridge.cpp
        ${HYPRVX_GENERATED_PROTOCOLS})
    target_include_directories(hyprvx_wayland_platform PUBLIC platform/include PRIVATE ${HYPRVX_PROTOCOL_DIR})
    target_link_libraries(hyprvx_wayland_platform PUBLIC hyprvx_nested_guard hyprvx_runtime_navigation
        PkgConfig::WAYLAND_CLIENT PkgConfig::XKBCOMMON)
    target_compile_features(hyprvx_wayland_platform PUBLIC cxx_std_20)

    add_executable(hyprvx_native_dev platform/src/native_dev_main.cpp)
    set_target_properties(hyprvx_native_dev PROPERTIES OUTPUT_NAME hyprvx-native-dev)
    target_link_libraries(hyprvx_native_dev PRIVATE hyprvx_wayland_platform)

    # Deterministic client for native tests (contracts/test-tooling.md section 4).
    add_executable(hyprvx_testclient tools/testclient/main.cpp ${HYPRVX_GENERATED_PROTOCOLS})
    set_target_properties(hyprvx_testclient PROPERTIES OUTPUT_NAME hyprvx-testclient)
    target_include_directories(hyprvx_testclient PRIVATE ${HYPRVX_PROTOCOL_DIR})
    target_link_libraries(hyprvx_testclient PRIVATE PkgConfig::WAYLAND_CLIENT)
    target_compile_features(hyprvx_testclient PRIVATE cxx_std_20)

    # Test-only virtual pointer for the nested compositor (contracts/test-tooling.md section 4a).
    hyprvx_client_protocol(wlr-virtual-pointer ${CMAKE_CURRENT_SOURCE_DIR}/tools/vpointer/protocols/wlr-virtual-pointer-unstable-v1.xml)
    add_executable(hyprvx_vpointer tools/vpointer/main.cpp
        ${HYPRVX_PROTOCOL_DIR}/wlr-virtual-pointer-protocol.c ${HYPRVX_PROTOCOL_DIR}/wlr-virtual-pointer-client-protocol.h)
    set_target_properties(hyprvx_vpointer PROPERTIES OUTPUT_NAME hyprvx-vpointer)
    target_include_directories(hyprvx_vpointer PRIVATE ${HYPRVX_PROTOCOL_DIR})
    target_link_libraries(hyprvx_vpointer PRIVATE PkgConfig::WAYLAND_CLIENT)
    target_compile_features(hyprvx_vpointer PRIVATE cxx_std_20)

    # Test-only virtual keyboard for the nested compositor (Feature 002 test-tooling 1.4 section 4b).
    hyprvx_client_protocol(virtual-keyboard-unstable-v1 ${CMAKE_CURRENT_SOURCE_DIR}/tools/vkeyboard/protocols/virtual-keyboard-unstable-v1.xml)
    add_executable(hyprvx_vkeyboard tools/vkeyboard/main.cpp
        ${HYPRVX_PROTOCOL_DIR}/virtual-keyboard-unstable-v1-protocol.c ${HYPRVX_PROTOCOL_DIR}/virtual-keyboard-unstable-v1-client-protocol.h)
    set_target_properties(hyprvx_vkeyboard PROPERTIES OUTPUT_NAME hyprvx-vkeyboard)
    target_include_directories(hyprvx_vkeyboard PRIVATE ${HYPRVX_PROTOCOL_DIR})
    target_link_libraries(hyprvx_vkeyboard PRIVATE PkgConfig::WAYLAND_CLIENT PkgConfig::XKBCOMMON)
    target_compile_features(hyprvx_vkeyboard PRIVATE cxx_std_20)

    # Deterministic X11 (XWayland) client for native tests (Feature 002 test-tooling 1.4 section 4c).
    pkg_check_modules(XCB REQUIRED IMPORTED_TARGET xcb)
    add_executable(hyprvx_x11client tools/x11client/main.cpp)
    set_target_properties(hyprvx_x11client PROPERTIES OUTPUT_NAME hyprvx-x11client)
    target_link_libraries(hyprvx_x11client PRIVATE PkgConfig::XCB)
    target_compile_features(hyprvx_x11client PRIVATE cxx_std_20)
endif()

if(HYPRVX_WITH_HYPRLAND_PLUGIN)
    pkg_check_modules(HYPRLAND REQUIRED IMPORTED_TARGET hyprland)
    add_library(hyprvx_hyprland_plugin MODULE platform/src/HyprlandPlugin.cpp)
    target_link_libraries(hyprvx_hyprland_plugin PRIVATE hyprvx_nested_guard PkgConfig::HYPRLAND)
    target_compile_features(hyprvx_hyprland_plugin PRIVATE cxx_std_23)
endif()

if(HYPRVX_WITH_SDL)
    pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3)

    add_executable(
        hyprvx_visual_simulator
        simulator/visual_main.cpp
        simulator/SdlNavigationInput.cpp
    )

    target_link_libraries(
        hyprvx_visual_simulator
        PRIVATE
            hyprvx_runtime_navigation
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
endif()

add_executable(
    hyprvx_ipc_watch
    daemon/src/ipc_watch_main.cpp
)

add_executable(
    hyprvx_project_replay
    daemon/src/project_replay_main.cpp
)

target_link_libraries(
    hyprvx_project_replay
    PRIVATE
        hyprvx_runtime_projection
        hyprvx_hyprland_ipc
)

set_target_properties(
    hyprvx_project_replay
    PROPERTIES
        OUTPUT_NAME hyprvx-project-replay
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
        tests/unit/test_camera_math.cpp
        tests/unit/test_drift_model.cpp
        tests/unit/test_gesture_model.cpp
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
        tests/unit/test_spatial_projection.cpp
        tests/unit/test_runtime_camera_controller.cpp
        tests/unit/test_runtime_navigation_input.cpp
        tests/unit/test_nested_guard.cpp
        tests/unit/test_projection_state.cpp
    )

    target_link_libraries(
        hyprvx_hyprland_ipc_tests
        PRIVATE
            hyprvx_runtime_navigation
            hyprvx_nested_guard
            hyprvx_runtime_projection
            hyprvx_hyprland_ipc
            GTest::gtest_main
    )
    if(HYPRVX_WITH_WAYLAND_PLATFORM)
        target_sources(hyprvx_hyprland_ipc_tests PRIVATE
            tests/unit/test_overlay_state.cpp
            tests/unit/test_native_event_bridge.cpp
        )
        target_link_libraries(hyprvx_hyprland_ipc_tests PRIVATE hyprvx_wayland_platform)
    endif()
    if(HYPRVX_WITH_HYPRLAND_PLUGIN)
        target_sources(hyprvx_hyprland_ipc_tests PRIVATE tests/unit/test_plugin_compatibility.cpp)
    endif()
    target_compile_definitions(hyprvx_hyprland_ipc_tests PRIVATE
        HYPRVX_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data"
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
