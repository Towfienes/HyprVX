// Form S mechanism API over the delivery spike's camera (Camera.cpp); see Api.hpp.

#include "SpikeAccess.hpp"

#include "Api.hpp"
#include "Camera.hpp"


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

// World offsets (Feature 002 T053): the plugin's port of patch 0010 (WindowOffset.cpp).
bool HyprVX::setWindowOffset(const PHLWINDOW& window, const Vector2D& offset) {
    return HyprVXSpike::setWindowOffset(window, offset);
}

Vector2D HyprVX::windowOffset(const PHLWINDOW& window) {
    return HyprVXSpike::windowOffset(window);
}

void HyprVX::resetWindowOffsets() {
    HyprVXSpike::resetWindowOffsets();
}
