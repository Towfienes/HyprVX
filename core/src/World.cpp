#include <utility>

#include "hyprvx/core/World.hpp"

namespace hyprvx {

bool World::addFreeWindow(
    WorldWindow window
) {
    return
        freeWindows_
        .emplace(
            window.id,
            window
        )
        .second;
}

bool World::removeFreeWindow(
    WindowId id
) {
    return
        freeWindows_.erase(id) != 0;
}

bool World::hasFreeWindow(
    WindowId id
) const noexcept {

    return freeWindows_.contains(id);
}

const WorldWindow* World::freeWindow(
    WindowId id
) const noexcept {

    const auto it =
        freeWindows_.find(id);

    return
        it == freeWindows_.end()
        ? nullptr
        : &it->second;
}

bool World::addIsland(
    Island island
) {
    return
        islands_
        .emplace(
            island.id(),
            std::move(island)
        )
        .second;
}

bool World::removeIsland(
    IslandId id
) {
    return
        islands_.erase(id) != 0;
}

const Island* World::island(
    IslandId id
) const noexcept {

    const auto it =
        islands_.find(id);

    return
        it == islands_.end()
        ? nullptr
        : &it->second;
}

Island* World::island(
    IslandId id
) noexcept {

    const auto it =
        islands_.find(id);

    return
        it == islands_.end()
        ? nullptr
        : &it->second;
}

}
