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
