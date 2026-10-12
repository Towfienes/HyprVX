// Window-behavior model (Feature 004, specs/004-drift-window-behavior/contracts/window-behavior.md 1.1).

#include "hyprvx/core/WindowBehavior.hpp"

#include "hyprvx/core/Rect.hpp"
#include "hyprvx/core/SpatialNavigation.hpp"
#include "hyprvx/core/ViewportFraming.hpp"
#include "hyprvx/core/Window.hpp"
#include "hyprvx/core/World.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <exception>

namespace hyprvx::drift {

namespace {

bool finite(double v) {
    return std::isfinite(v);
}

bool finiteVec(cammath::Vec v) {
    return finite(v.x) && finite(v.y);
}

bool finiteBox(const cammath::Box& b) {
    return finiteVec(b.pos) && finiteVec(b.size);
}

bool validCamera(const cammath::CameraState& c) {
    return finiteVec(c.origin) && finite(c.zoom) && c.zoom > 0.0;
}

bool validMonitor(cammath::Vec size) {
    return finiteVec(size) && size.x > 0.0 && size.y > 0.0;
}

bool validMargin(double margin) {
    return finite(margin) && margin >= 0.0 && margin <= 0.4;
}

bool validLimits(cammath::ZoomLimits limits) {
    return finite(limits.min) && finite(limits.max) && limits.min > 0.0 && limits.max >= limits.min;
}

// Axis accessors: 0 = x, 1 = y.
double lo(const cammath::Box& b, int axis) {
    return axis == 0 ? b.pos.x : b.pos.y;
}

double len(const cammath::Box& b, int axis) {
    return axis == 0 ? b.size.x : b.size.y;
}

double hi(const cammath::Box& b, int axis) {
    return lo(b, axis) + len(b, axis);
}

double comp(cammath::Vec v, int axis) {
    return axis == 0 ? v.x : v.y;
}

// Open intervals: touching edges do not overlap.
bool overlapOpen(const cammath::Box& a, const cammath::Box& b, int axis) {
    return std::min(hi(a, axis), hi(b, axis)) - std::max(lo(a, axis), lo(b, axis)) > 0.0;
}

bool facingAtGap(const cammath::Box& a, const cammath::Box& b, int axis, double gap, double tolerance) {
    if (!overlapOpen(a, b, 1 - axis))
        return false;
    return std::abs(lo(b, axis) - hi(a, axis) - gap) <= tolerance || std::abs(lo(a, axis) - hi(b, axis) - gap) <= tolerance;
}

const WorldBox* findBox(const std::vector<WorldBox>& boxes, WindowId id) {
    for (const auto& b : boxes) {
        if (b.id == id)
            return &b;
    }
    return nullptr;
}

hyprvx::CameraView toView(const cammath::CameraState& c) {
    return {hyprvx::Vec2{c.origin.x, c.origin.y}, c.zoom};
}

cammath::CameraState fromView(const hyprvx::CameraView& v) {
    return {{v.position.x, v.position.y}, v.zoom};
}

} // namespace

bool validSnapSettings(const SnapSettings& s) noexcept {
    return finite(s.threshold) && finite(s.gap) && finite(s.release) && s.threshold > 0.0 && s.gap >= 0.0 && s.release >= s.threshold;
}

SnapResult snapMove(const std::vector<WorldBox>& moving, cammath::Vec candidateDelta, const std::vector<WorldBox>& others, double zoom,
                    const SnapSettings& settings, const SnapState& previous) {
    const SnapResult unchanged{candidateDelta, {}};
    if (!settings.enabled || !validSnapSettings(settings) || !finite(zoom) || !(zoom > 0.0) || moving.empty() || !finiteVec(candidateDelta))
        return unchanged;
    for (const auto& b : moving) {
        if (!finiteBox(b.box))
            return unchanged;
    }
    // Snap targets: the other boxes, minus any moving window.
    std::vector<WorldBox> targets;
    targets.reserve(others.size());
    for (const auto& b : others) {
        if (!finiteBox(b.box))
            return unchanged;
        if (!findBox(moving, b.id))
            targets.push_back(b);
    }

    // The moving set's bounding box at the drag start and at the unsnapped candidate.
    double x0 = moving.front().box.pos.x, y0 = moving.front().box.pos.y;
    double x1 = x0 + moving.front().box.size.x, y1 = y0 + moving.front().box.size.y;
    for (const auto& b : moving) {
        x0 = std::min(x0, b.box.pos.x);
        y0 = std::min(y0, b.box.pos.y);
        x1 = std::max(x1, b.box.pos.x + b.box.size.x);
        y1 = std::max(y1, b.box.pos.y + b.box.size.y);
    }
    const cammath::Box start{{x0, y0}, {x1 - x0, y1 - y0}};
    const cammath::Box candidate{{x0 + candidateDelta.x, y0 + candidateDelta.y}, start.size};

    // Screen distances in world units through the transform: the attach band and the zoomed-out release distance.
    const cammath::Vec band        = cammath::worldDelta(cammath::CameraState{{}, zoom}, {settings.threshold, settings.release});
    const double       attachBand  = band.x;
    const double       releaseBand = std::max(settings.release, band.y);

    const AxisSnap* prev[2] = {&previous.x, &previous.y};
    AxisSnap        axisState[2];

    // Gap step: hold a previous gap snap, else attach to the nearest facing edge.
    for (int axis = 0; axis < 2; ++axis) {
        const AxisSnap& p = *prev[axis];
        const double    u = lo(candidate, axis);
        if (p.attached && !p.aligned && findBox(targets, p.target) && std::abs(u - p.snapped) <= releaseBand) {
            axisState[axis] = p;
            continue;
        }
        std::optional<WindowId> preferred;
        if (p.attached)
            preferred = p.target;
        bool     found = false;
        double   bestErr = 0.0, bestSnapped = 0.0;
        WindowId bestId = 0;
        auto     consider = [&](double err, WindowId id, double snapped) {
            if (err > attachBand)
                return;
            bool better = !found || err < bestErr;
            if (found && err == bestErr && id != bestId) {
                const bool idPreferred   = preferred && id == *preferred;
                const bool bestPreferred = preferred && bestId == *preferred;
                better = idPreferred || (!bestPreferred && id < bestId);
            }
            if (better) {
                found       = true;
                bestErr     = err;
                bestId      = id;
                bestSnapped = snapped;
            }
        };
        for (const auto& t : targets) {
            if (!overlapOpen(candidate, t.box, 1 - axis))
                continue;
            // Moving set before the target, then after it.
            consider(std::abs(lo(t.box, axis) - hi(candidate, axis) - settings.gap), t.id, lo(t.box, axis) - settings.gap - len(start, axis));
            consider(std::abs(lo(candidate, axis) - hi(t.box, axis) - settings.gap), t.id, hi(t.box, axis) + settings.gap);
        }
        if (found)
            axisState[axis] = {true, false, bestId, bestSnapped};
    }

    // Align step: only beside a gap snap on the other axis, to that axis's target.
    AxisSnap result[2] = {axisState[0], axisState[1]};
    for (int axis = 0; axis < 2; ++axis) {
        if (axisState[axis].attached)
            continue;
        const AxisSnap& other = axisState[1 - axis];
        if (!other.attached || other.aligned)
            continue;
        const WorldBox* target = findBox(targets, other.target);
        if (!target)
            continue;
        const AxisSnap& p = *prev[axis];
        const double    u = lo(candidate, axis);
        if (p.attached && p.aligned && p.target == target->id && std::abs(u - p.snapped) <= releaseBand) {
            result[axis] = p;
            continue;
        }
        const double errLo = std::abs(u - lo(target->box, axis));
        const double errHi = std::abs(hi(candidate, axis) - hi(target->box, axis));
        if (std::min(errLo, errHi) <= attachBand) {
            const double snapped = errLo <= errHi ? lo(target->box, axis) : hi(target->box, axis) - len(start, axis);
            result[axis]         = {true, true, target->id, snapped};
        }
    }

    SnapResult out;
    out.state.x = result[0];
    out.state.y = result[1];
    out.delta.x = result[0].attached ? result[0].snapped - lo(start, 0) : comp(candidateDelta, 0);
    out.delta.y = result[1].attached ? result[1].snapped - lo(start, 1) : comp(candidateDelta, 1);
    return out;
}

std::vector<WindowId> groupOf(WindowId seed, const std::vector<WorldBox>& boxes, double gap, double tolerance) {
    if (!findBox(boxes, seed))
        return {};
    std::vector<WindowId> group{seed};
    std::deque<WindowId>  queue{seed};
    while (!queue.empty()) {
        const WorldBox* current = findBox(boxes, queue.front());
        queue.pop_front();
        for (const auto& b : boxes) {
            if (std::find(group.begin(), group.end(), b.id) != group.end())
                continue;
            if (facingAtGap(current->box, b.box, 0, gap, tolerance) || facingAtGap(current->box, b.box, 1, gap, tolerance)) {
                group.push_back(b.id);
                queue.push_back(b.id);
            }
        }
    }
    std::sort(group.begin(), group.end());
    return group;
}

std::optional<WindowId> neighbour(WindowId from, Direction direction, const std::vector<WorldBox>& boxes) {
    hyprvx::World world;
    for (const auto& b : boxes) {
        if (!finiteBox(b.box))
            continue;
        world.addFreeWindow(hyprvx::WorldWindow{b.id, hyprvx::Rect{hyprvx::Vec2{b.box.pos.x, b.box.pos.y}, hyprvx::Vec2{b.box.size.x, b.box.size.y}}});
    }
    hyprvx::SpatialDirection dir = hyprvx::SpatialDirection::Left;
    switch (direction) {
        case Direction::Left: dir = hyprvx::SpatialDirection::Left; break;
        case Direction::Right: dir = hyprvx::SpatialDirection::Right; break;
        case Direction::Up: dir = hyprvx::SpatialDirection::Up; break;
        case Direction::Down: dir = hyprvx::SpatialDirection::Down; break;
    }
    const auto result = hyprvx::SpatialNavigator{}.findNeighbor(world, from, dir);
    if (!result)
        return std::nullopt;
    return result->windowId;
}

cammath::CameraState frameCamera(const cammath::Box& world, cammath::Vec monitorSize, double margin, cammath::ZoomLimits limits) {
    const cammath::CameraState fallback{world.pos, 1.0};
    if (!finiteBox(world) || world.size.x < 0.0 || world.size.y < 0.0 || !validMonitor(monitorSize) || !validMargin(margin) || !validLimits(limits))
        return fallback;
    hyprvx::ViewportFramingPolicy policy;
    policy.marginPixels        = margin * std::min(monitorSize.x, monitorSize.y);
    policy.minimumZoom         = limits.min;
    policy.maximumZoom         = limits.max;
    policy.preserveCurrentZoom = false;
    try {
        const auto framed = hyprvx::frameRectInViewport(hyprvx::Rect{hyprvx::Vec2{world.pos.x, world.pos.y}, hyprvx::Vec2{world.size.x, world.size.y}},
                                                        hyprvx::Vec2{monitorSize.x, monitorSize.y}, 1.0, policy);
        return {{framed.cameraPosition.x, framed.cameraPosition.y}, framed.zoom};
    } catch (const std::exception&) {
        return fallback;
    }
}

cammath::CameraState centerCamera(const cammath::CameraState& camera, cammath::Vec monitorSize, const cammath::Box& world) {
    if (!validCamera(camera) || !validMonitor(monitorSize) || !finiteBox(world))
        return camera;
    // Screen position of the box centre relative to the monitor origin, then the pan that moves it to the screen centre.
    const cammath::Vec centre{world.pos.x + world.size.x / 2.0, world.pos.y + world.size.y / 2.0};
    const cammath::Vec shown = cammath::forward(camera, {0.0, 0.0}, centre);
    return cammath::panByScreen(camera, {monitorSize.x / 2.0 - shown.x, monitorSize.y / 2.0 - shown.y});
}

cammath::CameraState revealCamera(const cammath::CameraState& camera, cammath::Vec monitorSize, const cammath::Box& world, double margin,
                                  cammath::ZoomLimits limits) {
    if (!validCamera(camera) || !validMonitor(monitorSize) || !finiteBox(world) || !validMargin(margin) || !validLimits(limits))
        return camera;
    const double       inset = margin * std::min(monitorSize.x, monitorSize.y);
    const cammath::Box shown = cammath::forwardBox(camera, {0.0, 0.0}, world);
    if (shown.size.x > monitorSize.x - 2.0 * inset || shown.size.y > monitorSize.y - 2.0 * inset)
        return frameCamera(world, monitorSize, margin, limits);
    auto shift = [&](int axis) {
        const double low = inset, high = comp(monitorSize, axis) - inset;
        if (lo(shown, axis) < low)
            return low - lo(shown, axis);
        if (hi(shown, axis) > high)
            return high - hi(shown, axis);
        return 0.0;
    };
    const cammath::Vec d{shift(0), shift(1)};
    if (d.x == 0.0 && d.y == 0.0)
        return camera;
    return cammath::panByScreen(camera, d);
}

void Bookmarks::store(int slot, const cammath::CameraState& camera) {
    if (slot < 1 || slot > 4 || !validCamera(camera))
        return;
    slots_[static_cast<std::size_t>(slot - 1)] = camera;
}

std::optional<cammath::CameraState> Bookmarks::recall(int slot) const {
    if (slot < 1 || slot > 4)
        return std::nullopt;
    return slots_[static_cast<std::size_t>(slot - 1)];
}

hyprvx::CameraHistory makeViewHistory() {
    return hyprvx::CameraHistory{VIEW_HISTORY_CAPACITY};
}

bool historyRecord(hyprvx::CameraHistory& history, const cammath::CameraState& previous) {
    if (!validCamera(previous))
        return false;
    history.record(toView(previous));
    return true;
}

std::optional<cammath::CameraState> historyBack(hyprvx::CameraHistory& history, const cammath::CameraState& current) {
    if (!validCamera(current))
        return std::nullopt;
    const auto view = history.back(toView(current));
    if (!view)
        return std::nullopt;
    return fromView(*view);
}

std::optional<cammath::CameraState> historyForward(hyprvx::CameraHistory& history, const cammath::CameraState& current) {
    if (!validCamera(current))
        return std::nullopt;
    const auto view = history.forward(toView(current));
    if (!view)
        return std::nullopt;
    return fromView(*view);
}

} // namespace hyprvx::drift
