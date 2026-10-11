# HyprVX delivery spike (T066, nested-only): seams 0002-0009 as createFunctionHook hooks in a plugin for the
# stock-from-source Hyprland 0.56.2 prefix. Standalone project; configure with
#   PKG_CONFIG_PATH=$HOME/.cache/hyprvx/claude/prefix/stock/share/pkgconfig cmake -S platform/plugin-spike -B <dir>
# Compiler and flags follow the stock build (GCC, gnu++26, -DUSES_SYSTEMD -DHAS_EXECINFO, Release).
cmake_minimum_required(VERSION 3.27)
project(hyprvx_plugin_spike CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_EXTENSIONS ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(HYPRLAND REQUIRED IMPORTED_TARGET hyprland pixman-1 libdrm libinput wayland-server xkbcommon pangocairo libeis-1.0)

add_library(hyprvx_plugin_spike SHARED
    src/main.cpp
    src/Camera.cpp
    src/DevControl.cpp
    src/RenderHooks.cpp
    src/copied/DragController.cpp
    src/copied/ElementRenderer.cpp
    src/copied/InputManager.cpp
    src/copied/OpenGL.cpp
    src/copied/Popup.cpp
    src/copied/Renderer.cpp
    src/copied/ViewHitTester.cpp
    src/copied/Window.cpp
)
target_include_directories(hyprvx_plugin_spike PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/include)
target_compile_definitions(hyprvx_plugin_spike PRIVATE USES_SYSTEMD HAS_EXECINFO WITH_GZFILEOP NOMINMAX)
target_compile_options(hyprvx_plugin_spike PRIVATE -frtti -fno-lto -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
                                                   -Wno-narrowing -Wno-pointer-arith -Wno-missing-braces)
target_link_libraries(hyprvx_plugin_spike PRIVATE PkgConfig::HYPRLAND)
set_target_properties(hyprvx_plugin_spike PROPERTIES PREFIX "" OUTPUT_NAME hyprvx-plugin-spike)
