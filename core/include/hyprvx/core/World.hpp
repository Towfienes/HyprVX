#pragma once

#include <unordered_map>

#include "Island.hpp"

namespace hyprvx {

class World {
public:
    bool addFreeWindow(WorldWindow window);

    bool removeFreeWindow(WindowId id);

    [[nodiscard]]
    bool hasFreeWindow(WindowId id) const noexcept;

    [[nodiscard]]
    const WorldWindow* freeWindow(
        WindowId id
    ) const noexcept;

    bool addIsland(Island island);

    bool removeIsland(IslandId id);

    [[nodiscard]]
    const Island* island(
        IslandId id
    ) const noexcept;

    [[nodiscard]]
    Island* island(
        IslandId id
    ) noexcept;

    [[nodiscard]]
    std::size_t freeWindowCount() const noexcept {
        return freeWindows_.size();
    }

    [[nodiscard]]
    std::size_t islandCount() const noexcept {
        return islands_.size();
    }

private:
    std::unordered_map<WindowId, WorldWindow> freeWindows_{};
    std::unordered_map<IslandId, Island> islands_{};
};

}
