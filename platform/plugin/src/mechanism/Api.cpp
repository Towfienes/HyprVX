// Form S mechanism API over the delivery spike's camera (Camera.cpp); see Api.hpp.

#include "SpikeAccess.hpp"

#include "Api.hpp"
#include "Camera.hpp"

#include <src/debug/log/Logger.hpp>

hyprvx::cammath::Vec HyprVX::monitorOrigin(const Monitor::CMonitor& monitor) {
    return HyprVXSpike::monitorOrigin(monitor);
}

hyprvx::cammath::CameraState HyprVX::cameraOf(const Monitor::CMonitor& monitor) {
    return HyprVXSpike::cameraOf(monitor);
}

bool HyprVX::cameraIsIdentity(const Monitor::CMonitor& monitor) {
    return HyprVXSpike::cameraIsIdentity(monitor);
}

bool HyprVX::setCamera(Monitor::CMonitor& monitor, const hyprvx::cammath::CameraState& camera) {
    return HyprVXSpike::setCamera(monitor, camera);
}

void HyprVX::resetCamera(Monitor::CMonitor& monitor) {
    HyprVXSpike::resetCamera(monitor);
}

// Offsets in form S are T053; until then a non-zero offset is refused, never silently dropped.
bool HyprVX::setWindowOffset(const PHLWINDOW&, const Vector2D& offset) {
    if (offset == Vector2D{})
        return true;
    static bool logged = false;
    if (!logged)
        Log::logger->log(Log::ERR, "[hyprvx] form S: window offsets are not implemented yet (T053); offset refused");
    logged = true;
    return false;
}

Vector2D HyprVX::windowOffset(const PHLWINDOW&) {
    return {};
}

void HyprVX::resetWindowOffsets() {}
