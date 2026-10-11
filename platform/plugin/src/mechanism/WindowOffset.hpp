#pragma once

// Form S world offsets (Feature 002 T053): patch 0010's WindowOffset and the 0010/0012 SceneCamera and InputCamera
// additions rebuilt in the plugin, under the names the generated copies use after gen_copies.py renames HyprVX:: to
// HyprVXSpike::. Same rules as contracts/mechanism-api.md 1.0; the form S mechanism API (Api.cpp) forwards to them.

#include "SpikeAccess.hpp"

#include "hyprvx/core/CameraMath.hpp"

#include <src/desktop/DesktopTypes.hpp>
#include <src/helpers/math/Math.hpp>
#include <src/helpers/memory/Memory.hpp>
#include <src/render/types.hpp>

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

class CWLSurfaceResource;

namespace Monitor {
    class CMonitor;
}

namespace HyprVXSpike {
    // ---- offsets (patch 0010 WindowOffset) ----
    bool      setWindowOffset(const PHLWINDOW& window, const Vector2D& offset);
    Vector2D  windowOffset(const PHLWINDOW& window);
    PHLWINDOW contentOwner(const SP<CWLSurfaceResource>& surface);
    Vector2D  contentOffset(const SP<CWLSurfaceResource>& surface);
    void      resetWindowOffsets();
    Vector2D  appliedOffset(const Desktop::View::CWindow& window);
    Vector2D  appliedContentOffset(const SP<CWLSurfaceResource>& surface);
    bool      anyWindowOffset();
    void      addOffsetDamage(Monitor::CMonitor& monitor, const CBox& globalLogical);
    // Every window with a non-zero offset (dev control state).
    std::vector<std::pair<PHLWINDOW, Vector2D>> windowOffsets();

    // ---- window pass (patch 0010 SceneCamera) ----
    struct SWindowPass {
        bool                                        active = false;
        std::optional<hyprvx::cammath::CameraState> camera;
        Render::SRenderModifData                    outer;
    };
    SWindowPass&             windowPass();
    Render::SRenderModifData windowRenderModif(const Monitor::CMonitor& monitor, const Vector2D& offset);
    Render::SRenderModifData passRenderModif(const Monitor::CMonitor& monitor);
    void                     notePassElements(Monitor::CMonitor& monitor, uint64_t count);

    // ---- input (patch 0012 InputCamera) ----
    Vector2D windowLocalPoint(const Desktop::View::CWindow& window, const Vector2D& screen);
}
