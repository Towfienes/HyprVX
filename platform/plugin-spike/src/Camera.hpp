#pragma once

// HyprVX delivery spike (T066): the per-monitor camera of patches 0001/0002/0004/0006 rebuilt for a plugin on stock
// Hyprland 0.56.2. A plugin cannot add members to CMonitor or SRenderModifData, so the camera lives in a map keyed by
// monitor id, and the camera render hint is recognised by a marker modifier (see cameraRenderModif).

#include "SpikeAccess.hpp"

#include "hyprvx/core/CameraMath.hpp"

#include <src/helpers/math/Math.hpp>
#include <src/helpers/memory/Memory.hpp>
#include <src/render/types.hpp>

#include <cstdint>
#include <optional>

class CWLSurfaceResource;

namespace Monitor {
    class CMonitor;
}

namespace HyprVXSpike {
    struct SMonitorCamera {
        hyprvx::cammath::CameraState state;
        bool                         active            = false;
        uint64_t                     revision          = 0;
        uint64_t                     passElementsAdded = 0;
    };

    inline constexpr hyprvx::cammath::ZoomLimits CAMERA_ZOOM_LIMITS{};

    SMonitorCamera&                              stored(const Monitor::CMonitor& monitor);
    hyprvx::cammath::Vec                         monitorOrigin(const Monitor::CMonitor& monitor);
    hyprvx::cammath::CameraState                 cameraOf(const Monitor::CMonitor& monitor);
    bool                                         cameraIsIdentity(const Monitor::CMonitor& monitor);
    bool                                         setCamera(Monitor::CMonitor& monitor, const hyprvx::cammath::CameraState& camera);
    void                                         resetCamera(Monitor::CMonitor& monitor);

    std::optional<hyprvx::cammath::CameraState>  activeCamera(const Monitor::CMonitor& monitor);

    // The patch marks its hint with SRenderModifData::hyprvxCamera. Here the hint starts with a zero TRANSLATE (a no-op
    // that stock never emits: renderAllClientsForWorkspace adds TRANSLATE only for a non-zero offset).
    Render::SRenderModifData cameraRenderModif(const Monitor::CMonitor& monitor, const hyprvx::cammath::CameraState& camera, const Render::SRenderModifData& outer);
    bool                     isCameraModif(const Render::SRenderModifData& modif);

    Hyprutils::Math::CBox    roundOutBox(const Hyprutils::Math::CBox& box);
    Hyprutils::Math::CBox    roundInBox(const Hyprutils::Math::CBox& box);
    bool                     cameraShows(const Monitor::CMonitor& monitor, const CBox& globalLogical);
    void                     addCameraDamage(Monitor::CMonitor& monitor, const CBox& globalLogical);
    void                     addCameraDamage(Monitor::CMonitor& monitor, const CRegion& globalLogical);

    Vector2D                 screenToWorld(const Vector2D& screen);
    Vector2D                 screenToWorld(const Monitor::CMonitor& monitor, const Vector2D& screen);
    Vector2D                 worldToScreen(const Monitor::CMonitor& monitor, const Vector2D& world);
    bool                     isWindowContent(const SP<CWLSurfaceResource>& surface);
}
