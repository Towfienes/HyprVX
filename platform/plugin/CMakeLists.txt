# HyprVX Drift policy plugin `hyprvx-drift` (Feature 002, research R3). One source set, two delivery forms:
#   HYPRVX_PLUGIN_FORM=P  policy only, for the patched Hyprland build; the mechanism API comes from the patched binary.
#   HYPRVX_PLUGIN_FORM=S  policy plus the mechanism as function hooks (src/mechanism, from the Feature 001 delivery
#                         spike), for stock Hyprland (DV3-16).
# HYPRVX_PLUGIN_DEV=ON adds the dev-only control (`hyprctl hyprvx-dev`) to form S for nested tests; form P gets it from
# the -DHYPRVX_DEV=ON patched build. A delivered build never has it (FR-016).
# Configure through scripts/build-plugin.sh, which points PKG_CONFIG_PATH at the matching agent prefix:
#   PKG_CONFIG_PATH=<prefix>/share/pkgconfig cmake -S platform/plugin -B <dir> -DHYPRVX_PLUGIN_FORM=P|S
# Compiler and flags follow the Hyprland build (GCC, gnu++26, -DUSES_SYSTEMD -DHAS_EXECINFO, Release).
cmake_minimum_required(VERSION 3.27)
project(hyprvx_drift CXX)

set(HYPRVX_PLUGIN_FORM "" CACHE STRING "Delivery form: P (patched Hyprland) or S (stock Hyprland + mechanism hooks)")
set(HYPRVX_PATCHSET_ID "none" CACHE STRING "Patchset id of the patched build form P is made for (ENTERING validation, research R10)")
option(HYPRVX_PLUGIN_DEV "Form S only: include the dev-only control for nested tests" OFF)
if(NOT HYPRVX_PLUGIN_FORM STREQUAL "P" AND NOT HYPRVX_PLUGIN_FORM STREQUAL "S")
    message(FATAL_ERROR "set -DHYPRVX_PLUGIN_FORM=P or -DHYPRVX_PLUGIN_FORM=S")
endif()
if(HYPRVX_PLUGIN_DEV AND NOT HYPRVX_PLUGIN_FORM STREQUAL "S")
    message(FATAL_ERROR "HYPRVX_PLUGIN_DEV applies to form S only (form P uses the HYPRVX_DEV patched build)")
endif()

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_EXTENSIONS ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(HYPRLAND REQUIRED IMPORTED_TARGET hyprland pixman-1 libdrm libinput wayland-server xkbcommon pangocairo libeis-1.0)

set(HYPRVX_REPO ${CMAKE_CURRENT_SOURCE_DIR}/../..)
file(GLOB HYPRVX_POLICY_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/policy/*.cpp)
# Core sources the policy compiles: DriftModel (Feature 002); GestureModel with the V1 Camera/CameraController it builds
# on (Feature 003).
set(HYPRVX_SOURCES ${HYPRVX_POLICY_SOURCES} ${HYPRVX_REPO}/core/src/DriftModel.cpp ${HYPRVX_REPO}/core/src/GestureModel.cpp
    ${HYPRVX_REPO}/core/src/Camera.cpp ${HYPRVX_REPO}/core/src/CameraMotion.cpp)

if(HYPRVX_PLUGIN_FORM STREQUAL "S")
    file(GLOB HYPRVX_MECHANISM_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/mechanism/*.cpp
         ${CMAKE_CURRENT_SOURCE_DIR}/src/mechanism/copied/*.cpp)
    list(APPEND HYPRVX_SOURCES ${HYPRVX_MECHANISM_SOURCES})
endif()

add_library(hyprvx_drift SHARED ${HYPRVX_SOURCES})
target_include_directories(hyprvx_drift PRIVATE ${HYPRVX_REPO}/core/include ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(hyprvx_drift PRIVATE USES_SYSTEMD HAS_EXECINFO WITH_GZFILEOP NOMINMAX HYPRVX_FORM_${HYPRVX_PLUGIN_FORM}
                                                HYPRVX_FORM_NAME="${HYPRVX_PLUGIN_FORM}" HYPRVX_PATCHSET_ID="${HYPRVX_PATCHSET_ID}")
if(HYPRVX_PLUGIN_DEV)
    target_compile_definitions(hyprvx_drift PRIVATE HYPRVX_DEV)
endif()
target_compile_options(hyprvx_drift PRIVATE -frtti -fno-lto -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
                                            -Wno-narrowing -Wno-pointer-arith -Wno-missing-braces)
target_link_libraries(hyprvx_drift PRIVATE PkgConfig::HYPRLAND)
set_target_properties(hyprvx_drift PROPERTIES PREFIX "" OUTPUT_NAME hyprvx-drift-${HYPRVX_PLUGIN_FORM})
