#pragma once

// Form S mechanism API (contracts/mechanism-api.md 1.0): the functions the policy calls, with the same names and
// signatures as the patched Hyprland binary exports from src/hyprvx/ (form P), implemented here over the delivery
// spike's per-monitor camera (Camera.hpp). The policy includes this header only through src/policy/Mechanism.hpp.

#include "hyprvx/core/CameraMath.hpp"

namespace Monitor {
    class CMonitor;
}

namespace HyprVX {
    // Camera (patch 0001).
    hyprvx::cammath::Vec         monitorOrigin(const Monitor::CMonitor& monitor);
    hyprvx::cammath::CameraState cameraOf(const Monitor::CMonitor& monitor);
    bool                         cameraIsIdentity(const Monitor::CMonitor& monitor);
    bool                         setCamera(Monitor::CMonitor& monitor, const hyprvx::cammath::CameraState& camera);
    void                         resetCamera(Monitor::CMonitor& monitor);
}
