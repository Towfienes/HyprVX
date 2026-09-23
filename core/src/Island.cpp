#include <algorithm>

#include "hyprvx/core/Island.hpp"

namespace hyprvx {

Island::Island(
    IslandId id,
    Vec2 origin
)
    : id_(id),
      origin_(origin) {}

bool Island::addMember(
    IslandMember member
) {
    if (findMember(member.id) != nullptr) {
        return false;
    }

    members_.push_back(member);
    return true;
}

bool Island::removeMember(
    WindowId windowId
) {
    const auto it =
        std::find_if(
            members_.begin(),
            members_.end(),
            [windowId](const IslandMember& member) {
                return member.id == windowId;
            }
        );

    if (it == members_.end()) {
        return false;
    }

    members_.erase(it);
    return true;
}

const IslandMember* Island::findMember(
    WindowId windowId
) const noexcept {

    const auto it =
        std::find_if(
            members_.begin(),
            members_.end(),
            [windowId](const IslandMember& member) {
                return member.id == windowId;
            }
        );

    return
        it == members_.end()
        ? nullptr
        : &*it;
}

std::optional<Rect>
Island::memberWorldGeometry(
    WindowId windowId
) const noexcept {

    const IslandMember* member =
        findMember(windowId);

    if (member == nullptr) {
        return std::nullopt;
    }

    return member
        ->localGeometry
        .translated(origin_);
}

std::optional<Rect>
Island::worldBounds() const noexcept {

    if (members_.empty()) {
        return std::nullopt;
    }

    Rect bounds =
        members_
        .front()
        .localGeometry
        .translated(origin_);

    for (
        std::size_t i = 1;
        i < members_.size();
        ++i
    ) {
        bounds =
            Rect::united(
                bounds,
                members_[i]
                    .localGeometry
                    .translated(origin_)
            );
    }

    return bounds;
}

}
