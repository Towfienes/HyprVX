#pragma once

// HyprVX Drift policy plugin `hyprvx-drift` (Feature 002, research R3): shared declarations of the policy adapter.
// The pure policy is hyprvx::drift::DriftModel (core); this adapter maps it to Hyprland objects, pushes cameras and
// offsets to the mechanism (Mechanism.hpp) and owns the Drift controls (contracts/drift-control.md 1.0).

#include "hyprvx/core/DriftModel.hpp"

#include <src/plugins/PluginAPI.hpp>

#include <string>

namespace HyprVXPolicy {
    inline HANDLE PHANDLE = nullptr;

    // ---- build validation (research R10; Compat.cpp) ----
    struct SBuildCheck {
        bool        ok = false;
        std::string expected;
        std::string running;
    };
    // At load: form S needs the plugin API hash of its headers and refuses a patched build (its mechanism would be
    // hooked twice); form P is bound to the patched binary by its undefined mechanism symbols.
    SBuildCheck checkLoad(HANDLE handle);
    // ENTERING (R10): form P, the patchset id in GIT_DIRTY (hyprvx-<id>) is the one this plugin was built against; form
    // S, the running commit is efb5099 and the build is clean upstream.
    SBuildCheck checkEntering(HANDLE handle);
    // {"form":…,"patchset":…,"hyprland":…} for `hyprctl hyprvx state`.
    std::string buildJSON(HANDLE handle);

    // ---- session (Session.cpp) ----
    void initSession(HANDLE handle);
    // Plugin unload: acts as panic (identity camera, mechanism offsets 0) before the hooks go away.
    void shutdownSession();
    // True while any monitor is in DRIFT: the input listeners return at once when false (FR-005).
    bool anyDrift();
    // True while the monitor under the pointer is in DRIFT (built-in inputs, drift-control 1.0).
    bool driftUnderPointer();
    // hyprvx:off on the focused monitor (Super+Escape).
    void disableFocused();
    // The session's model; valid between initSession and shutdownSession.
    hyprvx::drift::DriftModel& model();
    // Pushes the model to the mechanism: cameras of every monitor and, unless camerasOnly, every window's offset.
    void sync(bool camerasOnly = false);
    // The monitor under the pointer when it is in DRIFT, else null.
    PHLMONITOR driftMonitorUnderPointer();

    // ---- camera controls (Camera.cpp) ----
    // hyprvx:pan/zoom/reset and plugin:hyprvx:zoom_step.
    void initCamera(HANDLE handle);
    void shutdownCamera();
    // Zoom of the monitor's scene camera by factor, anchored at a screen point (global logical); false if refused.
    bool zoomAt(const PHLMONITOR& monitor, const Vector2D& anchor, double factor);
    // plugin:hyprvx:zoom_step (> 1; 1.1 if the configured value is not).
    double zoomStep();

    // ---- window policy (Windows.cpp) ----
    // Offsets set when windows open: override-redirect inheritance (R9); spawn placement (R6) later.
    void initWindows();
    void shutdownWindows();

    // ---- built-in inputs (Input.cpp) ----
    void initInput();
    void shutdownInput();
}
