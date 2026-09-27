#include <cmath>
#include <stdexcept>

#include "hyprvx/core/IslandInteraction.hpp"

namespace hyprvx {

namespace {

Rect expanded(
    Rect rect,
    double amount
) {
    rect.position.x -= amount;
    rect.position.y -= amount;

    rect.size.x += amount * 2.0;
    rect.size.y += amount * 2.0;

    return rect;
}

}

IslandInteractionController::
IslandInteractionController(
    IslandInteractionPolicy policy
)
    : policy_(policy) {

    if (
        !std::isfinite(policy_.joinMargin) ||
        policy_.joinMargin < 0.0 ||
        !std::isfinite(
            policy_.joinDwellSeconds
        ) ||
        policy_.joinDwellSeconds < 0.0 ||
        !std::isfinite(
            policy_.detachMargin
        ) ||
        policy_.detachMargin < 0.0
    ) {
        throw std::invalid_argument(
            "invalid island interaction policy"
        );
    }
}

void IslandInteractionController::reset()
    noexcept {
    state_ = {};
}

void IslandInteractionController::
beginFreeWindow(
    WindowId windowId,
    IslandId islandId
) {
    state_ = {};

    state_.windowId = windowId;
    state_.islandId = islandId;
}

void IslandInteractionController::
beginIslandMember(
    WindowId windowId,
    IslandId islandId,
    Rect islandBounds
) {
    state_ = {};

    state_.windowId = windowId;
    state_.islandId = islandId;

    state_.detachReferenceBounds =
        islandBounds;
}

void IslandInteractionController::update(
    const World& world,
    double deltaSeconds
) {
    if (
        !std::isfinite(deltaSeconds) ||
        deltaSeconds < 0.0
    ) {
        return;
    }

    if (
        world.hasFreeWindow(
            state_.windowId
        )
    ) {
        static_cast<void>(updateJoin(
            world,
            deltaSeconds
        ));

        return;
    }

    const auto owner =
        world.islandForWindow(
            state_.windowId
        );

    if (
        owner.has_value() &&
        *owner == state_.islandId
    ) {
        static_cast<void>(updateDetach(world));
        return;
    }

    state_.phase =
        IslandInteractionPhase::Idle;
}

bool IslandInteractionController::
updateJoin(
    const World& world,
    double deltaSeconds
) {
    const WorldWindow* window =
        world.freeWindow(
            state_.windowId
        );

    const Island* island =
        world.island(
            state_.islandId
        );

    if (
        window == nullptr ||
        island == nullptr
    ) {
        state_.phase =
            IslandInteractionPhase::Idle;

        state_.candidateElapsed = 0.0;

        return false;
    }

    const auto bounds =
        island->worldBounds();

    if (!bounds.has_value()) {
        state_.phase =
            IslandInteractionPhase::Idle;

        state_.candidateElapsed = 0.0;

        return false;
    }

    const Rect magnetic =
        expanded(
            *bounds,
            policy_.joinMargin
        );

    if (
        !magnetic.contains(
            window->geometry.center()
        )
    ) {
        state_.phase =
            IslandInteractionPhase::Idle;

        state_.candidateElapsed = 0.0;

        return false;
    }

    state_.candidateElapsed +=
        deltaSeconds;

    if (
        state_.candidateElapsed >=
        policy_.joinDwellSeconds
    ) {
        state_.phase =
            IslandInteractionPhase::JoinReady;
    } else {
        state_.phase =
            IslandInteractionPhase::
                JoinCandidate;
    }

    return true;
}

bool IslandInteractionController::
updateDetach(
    const World& world
) {
    const Island* island =
        world.island(
            state_.islandId
        );

    if (
        island == nullptr ||
        !state_.detachReferenceBounds
            .has_value()
    ) {
        state_.phase =
            IslandInteractionPhase::Idle;

        return false;
    }

    const auto geometry =
        island->memberWorldGeometry(
            state_.windowId
        );

    if (!geometry.has_value()) {
        state_.phase =
            IslandInteractionPhase::Idle;

        return false;
    }

    const Rect keepZone =
        expanded(
            *state_.detachReferenceBounds,
            policy_.detachMargin
        );

    if (
        keepZone.contains(
            geometry->center()
        )
    ) {
        state_.phase =
            IslandInteractionPhase::Idle;

        return false;
    }

    state_.phase =
        IslandInteractionPhase::
            DetachCandidate;

    return true;
}

bool IslandInteractionController::commit(
    World& world
) {
    bool changed = false;

    if (
        state_.phase ==
        IslandInteractionPhase::JoinReady
    ) {
        changed =
            world.joinFreeWindowToIsland(
                state_.windowId,
                state_.islandId
            );
    } else if (
        state_.phase ==
        IslandInteractionPhase::
            DetachCandidate
    ) {
        changed =
            world.detachWindowFromIsland(
                state_.windowId,
                state_.islandId
            );
    }

    reset();

    return changed;
}

}
