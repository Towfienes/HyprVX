#pragma once

// HyprVX Drift Mode policy model (Feature 002, specs/002-multi-window-drift-mvp/data-model.md, research R3/R12).
// Pure C++20 and compositor-independent: the per-monitor state machine OFF/ENTERING/DRIFT/EXITING, a camera per
// workspace, the per-window world offset O(w), at most one drag, and spawn placement. All camera arithmetic goes
// through CameraMath (Constitution IV); the plugin adapts this model to Hyprland objects and pushes the results to the
// mechanism. Coordinates are Hyprland global logical units.

#include "hyprvx/core/CameraMath.hpp"
#include "hyprvx/core/WindowBehavior.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace hyprvx::drift {

using MonitorId = std::int64_t;
using WorkspaceId = std::int64_t;
using WindowId = std::uint64_t;

enum class Mode { Off, Entering, Drift, Exiting };

// Ok: the transition or change happened. NoOp: idempotent call with nothing to do. State: not allowed in the current
// state (camera command outside DRIFT, enable on a special workspace). Invalid: non-finite or out-of-range request,
// nothing changed. Incompatible: ENTERING validation failed, back to OFF.
enum class Status { Ok, NoOp, State, Invalid, Incompatible };

enum class DragKind { Pan, Move };

// Feature 004: a move drag carries every member with its offset at the press (one for Super + drag, the group for
// Shift + Super + drag) and its snap state.
struct DragMember {
    WindowId     window = 0;
    cammath::Vec startOffset;
};

// What a snapping drag motion needs from the adapter (window-behavior 1.1 snapMove): the members' world boxes at the
// press (L + start offset), the boxes they can snap to, the current zoom and the settings.
struct SnapInput {
    std::vector<WorldBox> moving;
    std::vector<WorldBox> others;
    double                zoom = 1.0;
    SnapSettings          settings;
};

struct Drag {
    DragKind kind = DragKind::Pan;
    MonitorId monitor = 0;
    WindowId window = 0; // MOVE only
    cammath::Vec startScreen;
    cammath::Vec startOffset;         // MOVE: O(w) at press
    cammath::CameraState startCamera; // camera at press (PAN moves it; MOVE divides by its zoom)
    WorkspaceId workspace = 0;
    std::vector<DragMember> members; // MOVE: every moving window (members[0..] includes `window`)
    SnapState               snap;    // MOVE with snapping
};

// MVP zoom range (data-model.md WorkspaceCamera).
inline constexpr cammath::ZoomLimits DRIFT_ZOOM_LIMITS{};

// Hyprland's id convention: special workspaces have ids in [SPECIAL_WORKSPACE_START, -2] = [-99, -2] (macros.hpp,
// WorkspaceQueryCore.cpp:67); regular ids are positive and named workspaces have ids <= -1337.
[[nodiscard]] constexpr bool isSpecialWorkspaceId(WorkspaceId workspace) noexcept {
    return workspace >= -99 && workspace <= -2;
}

class DriftModel {
public:
    // ---- state machine (per monitor) ----
    [[nodiscard]] Mode mode(MonitorId monitor) const;
    // The monitor's scene workspace, only while DRIFT.
    [[nodiscard]] std::optional<WorkspaceId> sceneWorkspace(MonitorId monitor) const;

    // The monitor's logical origin M, used where an unset workspace camera means identityFor(M) (pan, drags). The
    // adapter sets it before enable and on monitor layout changes; unknown monitors use (0, 0).
    void setMonitorOrigin(MonitorId monitor, cammath::Vec origin);
    [[nodiscard]] cammath::Vec originOf(MonitorId monitor) const;

    // OFF -> ENTERING with the monitor's active workspace. NoOp while ENTERING/DRIFT; State on a special workspace.
    Status enable(MonitorId monitor, WorkspaceId active, bool special);
    // ENTERING -> DRIFT when the compositor validated (R10), else -> OFF (Incompatible). NoOp outside ENTERING.
    Status entered(MonitorId monitor, bool valid);
    // DRIFT -> EXITING, ending any drag. NoOp while OFF/EXITING. ENTERING -> OFF directly.
    Status disable(MonitorId monitor);
    // EXITING -> OFF, after the adapter reset the mechanism. NoOp outside EXITING.
    Status exited(MonitorId monitor);
    // Every monitor OFF, every drag ended. Stored cameras and offsets are kept.
    void panic();
    // An invalid camera on the monitor while DRIFT: same as disable.
    Status invalidCamera(MonitorId monitor);
    // The monitor's session is dropped (OFF); a re-added monitor starts OFF.
    void monitorRemoved(MonitorId monitor);
    // The monitor's active workspace changed. While DRIFT a regular workspace becomes the scene and any drag ends;
    // special workspaces never become the scene.
    void workspaceActivated(MonitorId monitor, WorkspaceId workspace, bool special);

    // ---- cameras (per workspace) ----
    // Stored camera of a workspace, or identityFor(M) of its monitor.
    [[nodiscard]] cammath::CameraState camera(WorkspaceId workspace, cammath::Vec monitorOrigin) const;
    [[nodiscard]] std::optional<cammath::CameraState> storedCamera(WorkspaceId workspace) const;
    // Validated with DRIFT_ZOOM_LIMITS; rejected values leave the stored camera unchanged. Special workspaces never get
    // a camera (isSpecialWorkspaceId).
    bool setCamera(WorkspaceId workspace, const cammath::CameraState& camera);
    // The camera the mechanism must show on the monitor: the scene camera while DRIFT, identity otherwise.
    [[nodiscard]] cammath::CameraState monitorCamera(MonitorId monitor, cammath::Vec monitorOrigin) const;
    // DRIFT only, on the scene workspace's camera.
    Status pan(MonitorId monitor, cammath::Vec screenDelta);
    Status zoom(MonitorId monitor, cammath::Vec monitorOrigin, cammath::Vec anchor, double factor);
    Status reset(MonitorId monitor, cammath::Vec monitorOrigin);

    // ---- window offsets ----
    [[nodiscard]] cammath::Vec offset(WindowId window) const;
    // Finite values only; rejected values leave the offset unchanged.
    bool setOffset(WindowId window, cammath::Vec offset);
    [[nodiscard]] const std::map<WindowId, cammath::Vec>& offsets() const { return offsets_; }
    // Drops the window's offset; the window leaves a move drag (the drag ends with its last member).
    void windowClosed(WindowId window);
    // The window left its monitor's scene (another workspace): it leaves a move drag, keeping its current offset (the
    // drag ends with its last member).
    void windowLeftScene(WindowId window);
    // O(w) for mapped windows of a DRIFT scene, 0 for every other window.
    [[nodiscard]] cammath::Vec mechanismOffset(WindowId window, WorkspaceId workspace, MonitorId monitor, bool mapped) const;

    // ---- drags (at most one) ----
    bool beginPan(MonitorId monitor, cammath::Vec screen);
    bool beginMove(MonitorId monitor, WindowId window, cammath::Vec screen);
    // Feature 004: a move drag of several windows (pressed must be a member, no duplicates, at least one member).
    bool beginGroupMove(MonitorId monitor, WindowId pressed, const std::vector<WindowId>& members, cammath::Vec screen);
    // Applies the drag for the pointer at `screen`: MOVE O = startOffset + worldDelta(startCamera, S - startScreen) for
    // every member; PAN P = panByScreen(startCamera, S - startScreen).P. False without a drag or for a rejected value.
    bool dragMotion(cammath::Vec screen);
    // MOVE with snapping: the same world delta, adjusted by snapMove (the drag keeps the snap state), for every member.
    // A PAN drag ignores `snap`.
    bool dragMotion(cammath::Vec screen, const SnapInput& snap);
    // Normal end (button release): offsets are kept.
    void dragEnd();
    // Cancellation (FR-021): a move drag restores every member's start offset; a pan drag keeps its camera. The end
    // conditions disable, invalidCamera, panic, monitorRemoved and workspaceActivated cancel this way.
    void dragCancel();
    [[nodiscard]] const std::optional<Drag>& drag() const { return drag_; }

    // ---- spawn placement (R6) ----
    // centre(visibleWorldRect(C)) - centre(L).
    [[nodiscard]] static cammath::Vec spawnOffset(const cammath::CameraState& camera, cammath::Vec monitorSize, cammath::Box logical);

private:
    struct Session {
        cammath::Vec origin;
        Mode mode = Mode::Off;
        WorkspaceId pending = 0; // workspace captured by enable; the scene once DRIFT
        std::optional<WorkspaceId> scene;
    };

    [[nodiscard]] static Session offSession(cammath::Vec origin);
    [[nodiscard]] const Session* session(MonitorId monitor) const;
    void endDragOn(MonitorId monitor);
    bool applyMoveDelta(cammath::Vec worldDelta);

    std::map<MonitorId, Session> sessions_;
    std::map<WorkspaceId, cammath::CameraState> cameras_;
    std::map<WindowId, cammath::Vec> offsets_;
    std::optional<Drag> drag_;
};

} // namespace hyprvx::drift
