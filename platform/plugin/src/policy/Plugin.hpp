#pragma once

// HyprVX Drift policy plugin `hyprvx-drift` (Feature 002, research R3): shared declarations of the policy adapter.
// The pure policy is hyprvx::drift::DriftModel (core); this adapter maps it to Hyprland objects, pushes cameras and
// offsets to the mechanism (Mechanism.hpp) and owns the Drift controls (contracts/drift-control.md 1.0).

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
    // ENTERING: the running Hyprland commit is efb5099 and, in form P, its patchset id (GIT_DIRTY hyprvx-<id>) is the
    // one this plugin was built against; in form S it is a clean upstream build.
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

    // ---- built-in inputs (Input.cpp) ----
    void initInput();
    void shutdownInput();
}
