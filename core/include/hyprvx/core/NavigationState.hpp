#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "Vec2.hpp"

namespace hyprvx {

struct CameraView {
    Vec2 position{};
    double zoom{1.0};
};

using ZoneId = std::uint64_t;

struct Zone {
    ZoneId id{};
    std::string name{};
    CameraView view{};
};

class CameraHistory {
public:
    explicit CameraHistory(
        std::size_t capacity = 64
    );

    void record(
        CameraView view
    );

    [[nodiscard]]
    std::optional<CameraView> back(
        CameraView current
    );

    [[nodiscard]]
    std::optional<CameraView> forward(
        CameraView current
    );

    void clear() noexcept;

    [[nodiscard]]
    bool canGoBack() const noexcept {
        return !backStack_.empty();
    }

    [[nodiscard]]
    bool canGoForward() const noexcept {
        return !forwardStack_.empty();
    }

    [[nodiscard]]
    std::size_t backSize() const noexcept {
        return backStack_.size();
    }

    [[nodiscard]]
    std::size_t forwardSize() const noexcept {
        return forwardStack_.size();
    }

private:
    void trim(
        std::vector<CameraView>& stack
    );

    std::size_t capacity_{};

    std::vector<CameraView> backStack_{};
    std::vector<CameraView> forwardStack_{};
};

class CameraHistoryTransaction {
public:
    explicit CameraHistoryTransaction(
        CameraHistory& history
    ) noexcept;

    void begin(
        CameraView start
    );

    [[nodiscard]]
    bool finish(
        CameraView end
    );

    void cancel() noexcept;

    [[nodiscard]]
    bool active() const noexcept {
        return startView_.has_value();
    }

    [[nodiscard]]
    const std::optional<CameraView>&
    startView() const noexcept {
        return startView_;
    }

private:
    CameraHistory* history_{};
    std::optional<CameraView> startView_{};
};

class ZoneStore {
public:
    bool set(Zone zone);

    [[nodiscard]]
    const Zone* find(
        ZoneId id
    ) const noexcept;

    [[nodiscard]]
    Zone* find(
        ZoneId id
    ) noexcept;

    bool erase(
        ZoneId id
    );

    void clear() noexcept {
        zones_.clear();
    }

    [[nodiscard]]
    std::size_t size() const noexcept {
        return zones_.size();
    }

private:
    std::unordered_map<ZoneId, Zone>
        zones_{};
};

}
