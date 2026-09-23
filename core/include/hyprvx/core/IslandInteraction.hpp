#pragma once

#include <optional>

#include "World.hpp"

namespace hyprvx {

enum class IslandInteractionPhase {
    Idle,
    JoinCandidate,
    JoinReady,
    DetachCandidate
};

struct IslandInteractionPolicy {
    double joinMargin{220.0};
    double joinDwellSeconds{0.25};
    double detachMargin{160.0};
};

struct IslandInteractionState {
    IslandInteractionPhase phase{
        IslandInteractionPhase::Idle
    };

    WindowId windowId{};
    IslandId islandId{};

    double candidateElapsed{};

    std::optional<Rect> detachReferenceBounds{};
};

class IslandInteractionController {
public:
    explicit IslandInteractionController(
        IslandInteractionPolicy policy = {}
    );

    [[nodiscard]]
    const IslandInteractionPolicy&
    policy() const noexcept {
        return policy_;
    }

    [[nodiscard]]
    const IslandInteractionState&
    state() const noexcept {
        return state_;
    }

    void reset() noexcept;

    void beginFreeWindow(
        WindowId windowId,
        IslandId islandId
    );

    void beginIslandMember(
        WindowId windowId,
        IslandId islandId,
        Rect islandBounds
    );

    void update(
        const World& world,
        double deltaSeconds
    );

    [[nodiscard]]
    bool commit(World& world);

private:
    [[nodiscard]]
    bool updateJoin(
        const World& world,
        double deltaSeconds
    );

    [[nodiscard]]
    bool updateDetach(
        const World& world
    );

    IslandInteractionPolicy policy_{};
    IslandInteractionState state_{};
};

}
