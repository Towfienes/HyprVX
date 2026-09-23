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
    core/src/World.cpp
)

target_include_directories(
    hyprvx_core
    PUBLIC
        core/include
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

include(CTest)

if(BUILD_TESTING AND HYPRVX_BUILD_TESTS)
    find_package(GTest REQUIRED)

    add_executable(
        hyprvx_core_tests

        tests/unit/test_geometry.cpp
        tests/unit/test_camera.cpp
        tests/unit/test_camera_motion.cpp
        tests/unit/test_island.cpp
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
endif()
