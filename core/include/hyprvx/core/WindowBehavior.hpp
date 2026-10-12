#pragma once

// HyprVX window behavior in Drift (Feature 004, specs/004-drift-window-behavior/contracts/window-behavior.md 1.1):
// snap with gap, alignment and hysteresis, implicit groups, directional neighbours, framing/centering/reveal cameras,
// bookmarks and the view history. Pure C++20, no compositor types, no clock, no I/O. Units are world px (Hyprland
// global logical) unless named screen px. Camera results come from CameraMath (Constitution IV); the V1 navigator,
// viewport framing and camera history are reused through adapters (research R1, Constitution VIII).

#include "hyprvx/core/CameraMath.hpp"
#include "hyprvx/core/NavigationState.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace hyprvx::drift {

using WindowId = std::uint64_t;

struct SnapSettings {
    bool   enabled   = true;
    double threshold = 24.0; // screen px
    double gap       = 12.0; // world px
    double release   = 32.0; // world px (at zoom >= 1; release / zoom when zoomed out)
};

// Finite, threshold > 0, gap >= 0, release >= threshold.
[[nodiscard]] bool validSnapSettings(const SnapSettings& settings) noexcept;

struct WorldBox {
    WindowId     id = 0;
    cammath::Box box; // L(w) + O(w), the window's size
};

// One axis of a drag's snap: attached to `target` at `snapped` (the moving bounding-box origin on that axis), either by
// a gap snap or (aligned) by edge alignment next to a gap snap on the other axis.
struct AxisSnap {
    bool     attached = false;
    bool     aligned  = false;
    WindowId target   = 0;
    double   snapped  = 0.0;
};

struct SnapState {
    AxisSnap x, y;
};

struct SnapResult {
    cammath::Vec delta;
    SnapState    state;
};

// `moving`: the moving set's boxes at the drag start; `candidateDelta`: the unsnapped world delta; `others`: the boxes
// that can be snapped to. Disabled or invalid settings, non-finite input, zoom <= 0 or an empty moving set return
// {candidateDelta, {}}. Deterministic.
[[nodiscard]] SnapResult snapMove(const std::vector<WorldBox>& moving, cammath::Vec candidateDelta, const std::vector<WorldBox>& others, double zoom,
                                  const SnapSettings& settings, const SnapState& previous);

// The connected component of "facing edges at gap +- tolerance and overlapping on the other axis" containing `seed`,
// sorted by id; {seed} without a neighbour; empty when `seed` is not in `boxes`.
[[nodiscard]] std::vector<WindowId> groupOf(WindowId seed, const std::vector<WorldBox>& boxes, double gap, double tolerance = 1.0);

enum class Direction { Left, Right, Up, Down };

// V1 SpatialNavigator::findNeighbor with its default policy; nullopt when there is none or `from` is not in `boxes`.
[[nodiscard]] std::optional<WindowId> neighbour(WindowId from, Direction direction, const std::vector<WorldBox>& boxes);

// V1 frameRectInViewport (preserveCurrentZoom = false, margin = margin x min(monitor w, h) screen px): the box centre at
// the screen centre, zoom clamped to `limits`. Invalid input (non-finite, monitor size <= 0, margin outside [0, 0.4],
// bad limits) returns {box position, 1}.
[[nodiscard]] cammath::CameraState frameCamera(const cammath::Box& world, cammath::Vec monitorSize, double margin, cammath::ZoomLimits limits = {});

// Same zoom, the box centre at the screen centre. Invalid input returns `camera`.
[[nodiscard]] cammath::CameraState centerCamera(const cammath::CameraState& camera, cammath::Vec monitorSize, const cammath::Box& world);

// `camera` if the box lies within the margin inset of the screen; else the smallest pan that brings it inside; if it
// cannot fit at the current zoom, frameCamera. Invalid input returns `camera`.
[[nodiscard]] cammath::CameraState revealCamera(const cammath::CameraState& camera, cammath::Vec monitorSize, const cammath::Box& world, double margin,
                                                cammath::ZoomLimits limits = {});

// Slots 1-4; other slots are ignored on store and empty on recall.
class Bookmarks {
  public:
    void                                              store(int slot, const cammath::CameraState& camera);
    [[nodiscard]] std::optional<cammath::CameraState> recall(int slot) const;

  private:
    std::array<std::optional<cammath::CameraState>, 4> slots_{};
};

// View history: V1 CameraHistory, its CameraView holding CameraState's origin and zoom exactly.
inline constexpr std::size_t VIEW_HISTORY_CAPACITY = 32;

[[nodiscard]] hyprvx::CameraHistory makeViewHistory();
// False (nothing recorded) for a non-finite camera or zoom <= 0.
bool historyRecord(hyprvx::CameraHistory& history, const cammath::CameraState& previous);
[[nodiscard]] std::optional<cammath::CameraState> historyBack(hyprvx::CameraHistory& history, const cammath::CameraState& current);
[[nodiscard]] std::optional<cammath::CameraState> historyForward(hyprvx::CameraHistory& history, const cammath::CameraState& current);

} // namespace hyprvx::drift
