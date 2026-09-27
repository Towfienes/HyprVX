#pragma once

#include "hyprvx/runtime/ProjectionState.hpp"

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>

namespace hyprvx::runtime {

class SpatialProjector;

enum class StateLoadStatus {
    Loaded,
    RecoveredBackup,
    Missing,
    Corrupt,
    UnsupportedVersion,
    IoError
};

struct StateLoadResult {
    StateLoadStatus status{StateLoadStatus::Missing};
    std::optional<DurableProjectionState> state;
    std::string diagnostic;
};

class ProjectionStateStore {
public:
    [[nodiscard]] static std::filesystem::path defaultPath();
    [[nodiscard]] StateLoadResult load(const std::filesystem::path& path) const;

    // Invalid existing state is preserved unless the caller explicitly decides
    // to replace it after inspecting the diagnostic.
    void saveAtomic(
        const std::filesystem::path& path,
        const DurableProjectionState& state,
        bool replaceInvalidExisting = false) const;
};

class ProjectionStateManager {
public:
    using Clock = std::chrono::steady_clock;

    ProjectionStateManager(
        std::filesystem::path path,
        std::chrono::milliseconds debounce = std::chrono::milliseconds{750},
        std::chrono::milliseconds maxDirtyDelay = std::chrono::seconds{5});

    [[nodiscard]] StateLoadResult load() const;
    void observeLayout(const SpatialProjector& projector, Clock::time_point now);
    [[nodiscard]] bool flushIfDue(
        Clock::time_point now,
        const SpatialProjector& projector,
        bool replaceInvalidExisting = false);
    void saveNow(
        const SpatialProjector& projector,
        bool replaceInvalidExisting = false);
    [[nodiscard]] bool dirty() const noexcept { return dirty_; }
    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
    std::chrono::milliseconds debounce_;
    std::chrono::milliseconds maxDirtyDelay_;
    ProjectionStateStore store_;
    std::optional<Clock::time_point> deadline_;
    std::optional<Clock::time_point> firstDirtyAt_;
    std::uint64_t observedRevision_{};
    bool dirty_{};
};

} // namespace hyprvx::runtime
