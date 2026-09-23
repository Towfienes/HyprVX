#include <cmath>
#include <stdexcept>
#include <utility>

#include "hyprvx/core/NavigationState.hpp"

namespace hyprvx {

namespace {

void validateCameraView(
    const CameraView& view
) {
    if (
        !std::isfinite(view.position.x) ||
        !std::isfinite(view.position.y) ||
        !std::isfinite(view.zoom) ||
        !(view.zoom > 0.0)
    ) {
        throw std::invalid_argument(
            "invalid camera view"
        );
    }
}

bool sameCameraView(
    const CameraView& a,
    const CameraView& b
) {
    constexpr double positionEpsilon =
        1e-9;

    constexpr double zoomEpsilon =
        1e-12;

    return
        std::abs(
            a.position.x -
            b.position.x
        ) <= positionEpsilon &&
        std::abs(
            a.position.y -
            b.position.y
        ) <= positionEpsilon &&
        std::abs(
            a.zoom -
            b.zoom
        ) <= zoomEpsilon;
}

}

CameraHistory::CameraHistory(
    std::size_t capacity
)
    : capacity_(capacity) {

    if (capacity_ == 0) {
        throw std::invalid_argument(
            "camera history capacity must be positive"
        );
    }
}

void CameraHistory::trim(
    std::vector<CameraView>& stack
) {
    while (
        stack.size() >
        capacity_
    ) {
        stack.erase(
            stack.begin()
        );
    }
}

void CameraHistory::record(
    CameraView view
) {
    validateCameraView(view);

    if (
        !backStack_.empty() &&
        sameCameraView(
            backStack_.back(),
            view
        )
    ) {
        forwardStack_.clear();
        return;
    }

    backStack_.push_back(view);
    trim(backStack_);

    forwardStack_.clear();
}

std::optional<CameraView>
CameraHistory::back(
    CameraView current
) {
    validateCameraView(current);

    if (backStack_.empty()) {
        return std::nullopt;
    }

    const CameraView target =
        backStack_.back();

    backStack_.pop_back();

    if (
        !sameCameraView(
            current,
            target
        )
    ) {
        forwardStack_.push_back(current);
        trim(forwardStack_);
    }

    return target;
}

std::optional<CameraView>
CameraHistory::forward(
    CameraView current
) {
    validateCameraView(current);

    if (forwardStack_.empty()) {
        return std::nullopt;
    }

    const CameraView target =
        forwardStack_.back();

    forwardStack_.pop_back();

    if (
        !sameCameraView(
            current,
            target
        )
    ) {
        backStack_.push_back(current);
        trim(backStack_);
    }

    return target;
}

void CameraHistory::clear() noexcept {
    backStack_.clear();
    forwardStack_.clear();
}

CameraHistoryTransaction::
CameraHistoryTransaction(
    CameraHistory& history
) noexcept
    : history_(&history) {
}

void CameraHistoryTransaction::begin(
    CameraView start
) {
    validateCameraView(start);

    if (startView_.has_value()) {
        return;
    }

    startView_ = start;
}

bool CameraHistoryTransaction::finish(
    CameraView end
) {
    validateCameraView(end);

    if (!startView_.has_value()) {
        return false;
    }

    const CameraView start =
        *startView_;

    startView_.reset();

    if (
        sameCameraView(
            start,
            end
        )
    ) {
        return false;
    }

    history_->record(start);

    return true;
}

void CameraHistoryTransaction::cancel()
    noexcept {
    startView_.reset();
}

bool ZoneStore::set(
    Zone zone
) {
    if (
        zone.id == 0 ||
        zone.name.empty()
    ) {
        throw std::invalid_argument(
            "invalid zone"
        );
    }

    validateCameraView(
        zone.view
    );

    auto it =
        zones_.find(zone.id);

    if (it != zones_.end()) {
        it->second =
            std::move(zone);

        return false;
    }

    zones_.emplace(
        zone.id,
        std::move(zone)
    );

    return true;
}

const Zone* ZoneStore::find(
    ZoneId id
) const noexcept {
    const auto it =
        zones_.find(id);

    return
        it == zones_.end()
        ? nullptr
        : &it->second;
}

Zone* ZoneStore::find(
    ZoneId id
) noexcept {
    const auto it =
        zones_.find(id);

    return
        it == zones_.end()
        ? nullptr
        : &it->second;
}

bool ZoneStore::erase(
    ZoneId id
) {
    return zones_.erase(id) != 0;
}

}
