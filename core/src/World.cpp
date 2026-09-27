#include <utility>

#include "hyprvx/core/World.hpp"

namespace hyprvx {

bool World::addFreeWindow(
    WorldWindow window
) {
    if (windowExists(window.id)) {
        return false;
    }

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

bool World::removeWindow(WindowId id) {
    if (removeFreeWindow(id)) {
        return true;
    }
    for (auto& [islandId, islandValue] : islands_) {
        static_cast<void>(islandId);
        if (islandValue.removeMember(id)) {
            return true;
        }
    }
    return false;
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

WorldWindow* World::freeWindow(
    WindowId id
) noexcept {

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
    if (
        islands_.contains(
            island.id()
        )
    ) {
        return false;
    }

    for (
        const auto& member :
        island.members()
    ) {
        if (
            freeWindows_.contains(member.id) ||
            windowExistsInIslands(member.id)
        ) {
            return false;
        }
    }

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


bool World::windowExistsInIslands(
    WindowId id
) const noexcept {

    for (
        const auto& [islandId, islandValue] :
        islands_
    ) {
        static_cast<void>(islandId);

        if (
            islandValue.findMember(id) !=
            nullptr
        ) {
            return true;
        }
    }

    return false;
}

bool World::windowExists(
    WindowId id
) const noexcept {

    return
        freeWindows_.contains(id) ||
        windowExistsInIslands(id);
}

std::optional<IslandId>
World::islandForWindow(
    WindowId id
) const noexcept {

    for (
        const auto& [islandId, islandValue] :
        islands_
    ) {
        if (
            islandValue.findMember(id) !=
            nullptr
        ) {
            return islandId;
        }
    }

    return std::nullopt;
}


std::optional<Rect>
World::windowGeometry(
    WindowId id
) const noexcept {

    const auto freeIt =
        freeWindows_.find(id);

    if (
        freeIt !=
        freeWindows_.end()
    ) {
        return
            freeIt->second.geometry;
    }

    for (
        const auto& [islandId, islandValue] :
        islands_
    ) {
        static_cast<void>(islandId);

        const auto geometry =
            islandValue.memberWorldGeometry(
                id
            );

        if (geometry.has_value()) {
            return geometry;
        }
    }

    return std::nullopt;
}

bool World::joinFreeWindowToIsland(
    WindowId windowId,
    IslandId islandId
) {
    auto windowIt =
        freeWindows_.find(windowId);

    auto islandIt =
        islands_.find(islandId);

    if (
        windowIt == freeWindows_.end() ||
        islandIt == islands_.end()
    ) {
        return false;
    }

    const WorldWindow window =
        windowIt->second;

    const Vec2 localPosition =
        window.geometry.position -
        islandIt->second.origin();

    IslandMember member{
        window.id,
        {
            localPosition,
            window.geometry.size
        }
    };

    if (
        !islandIt->second.addMember(
            member
        )
    ) {
        return false;
    }

    freeWindows_.erase(windowIt);

    return true;
}

bool World::detachWindowFromIsland(
    WindowId windowId,
    IslandId islandId
) {
    auto islandIt =
        islands_.find(islandId);

    if (
        islandIt == islands_.end() ||
        freeWindows_.contains(windowId)
    ) {
        return false;
    }

    const IslandMember* member =
        islandIt->second.findMember(
            windowId
        );

    if (member == nullptr) {
        return false;
    }

    const IslandMember memberCopy =
        *member;

    const auto worldGeometry =
        islandIt->second
            .memberWorldGeometry(
                windowId
            );

    if (!worldGeometry.has_value()) {
        return false;
    }

    if (
        !islandIt->second.removeMember(
            windowId
        )
    ) {
        return false;
    }

    const bool inserted =
        freeWindows_
        .emplace(
            windowId,
            WorldWindow{
                windowId,
                *worldGeometry
            }
        )
        .second;

    if (!inserted) {
        islandIt->second.addMember(
            memberCopy
        );

        return false;
    }

    return true;
}

}
