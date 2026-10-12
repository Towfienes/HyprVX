# Test-only Hyprland plugin `hyprvx-testinput` (Feature 003, contracts/test-tooling.md 1.5 §8): hyprctl `hvxtest` for
# synthetic swipe/pinch gestures through CInputManager and a render.pre frame counter. Never part of a delivery form;
# loaded only by the harness into verified nested sessions. Built per form against the matching agent prefix:
#   HYPRVX_TESTINPUT_FORM=P  patched prefix;  HYPRVX_TESTINPUT_FORM=S  stock prefix.
# Configure through scripts/build-plugin.sh --testinput --form P|S (PKG_CONFIG_PATH points at the prefix).
# Compiler and flags follow platform/plugin/CMakeLists.txt (GCC, gnu++26, Hyprland's defines, Release).
cmake_minimum_required(VERSION 3.27)
project(hyprvx_testinput CXX)

set(HYPRVX_TESTINPUT_FORM "" CACHE STRING "Form whose Hyprland build this test plugin is for: P or S")
if(NOT HYPRVX_TESTINPUT_FORM STREQUAL "P" AND NOT HYPRVX_TESTINPUT_FORM STREQUAL "S")
    message(FATAL_ERROR "set -DHYPRVX_TESTINPUT_FORM=P or -DHYPRVX_TESTINPUT_FORM=S")
endif()

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_EXTENSIONS ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(HYPRLAND REQUIRED IMPORTED_TARGET hyprland pixman-1 libdrm libinput wayland-server xkbcommon pangocairo libeis-1.0)

add_library(hyprvx_testinput SHARED ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)
target_compile_definitions(hyprvx_testinput PRIVATE USES_SYSTEMD HAS_EXECINFO WITH_GZFILEOP NOMINMAX)
target_compile_options(hyprvx_testinput PRIVATE -frtti -fno-lto -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
                                                -Wno-narrowing -Wno-pointer-arith -Wno-missing-braces)
target_link_libraries(hyprvx_testinput PRIVATE PkgConfig::HYPRLAND)
set_target_properties(hyprvx_testinput PROPERTIES PREFIX "" OUTPUT_NAME hyprvx-testinput-${HYPRVX_TESTINPUT_FORM})
