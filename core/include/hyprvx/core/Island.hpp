#pragma once

#include <optional>
#include <vector>

#include "Types.hpp"
#include "Window.hpp"

namespace hyprvx {

class Island {
public:
    explicit Island(
        IslandId id,
        Vec2 origin = {}
    );

    [[nodiscard]]
    IslandId id() const noexcept {
        return id_;
    }

    [[nodiscard]]
    const Vec2& origin() const noexcept {
        return origin_;
    }

    [[nodiscard]]
    const std::vector<IslandMember>& members() const noexcept {
        return members_;
    }

    void setOrigin(Vec2 origin) noexcept {
        origin_ = origin;
    }

    void moveBy(Vec2 delta) noexcept {
        origin_ += delta;
    }

    bool addMember(IslandMember member);

    bool removeMember(WindowId windowId);

    [[nodiscard]]
    const IslandMember* findMember(
        WindowId windowId
    ) const noexcept;

    [[nodiscard]]
    std::optional<Rect> memberWorldGeometry(
        WindowId windowId
    ) const noexcept;

    [[nodiscard]]
    std::optional<Rect> worldBounds() const noexcept;

private:
    IslandId id_{};
    Vec2 origin_{};
    std::vector<IslandMember> members_{};
};

}
