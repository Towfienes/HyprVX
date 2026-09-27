#pragma once

#include "hyprvx/core/Types.hpp"
#include "hyprvx/core/Rect.hpp"

#include <cstdint>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace hyprvx::runtime {

inline constexpr std::uint32_t ProjectionStateSchemaVersion = 1;
// Persisted coordinates are capped to keep rectangle and placement arithmetic
// safely bounded while the in-memory world remains unrestricted by this policy.
inline constexpr double ProjectionPersistenceWorldLimit = 1.0e12;

struct InitialPlacementPolicy {
    Vec2 windowSize{1000.0, 700.0};
    double workspaceStride{3600.0};
    std::size_t columns{3};
    // Finite initial vertical extent; bounds grow deterministically as rows are used.
    std::size_t initialRegionRows{3};
    double gap{200.0};
    Vec2 firstWorkspaceOrigin{};
};

struct PersistedWorkspaceRegion {
    std::string name;
    Vec2 origin{};
    Rect bounds{};
    std::optional<int> workspaceId;
};

struct PersistedWindowLayout {
    std::string address;
    std::string compositorEpoch;
    WindowId windowId{};
    std::optional<std::string> className;
    std::optional<std::string> title;
    std::optional<int> workspaceId;
    std::optional<std::string> workspaceName;
    std::optional<bool> floating;
    Rect geometry{};
    std::uint64_t placementSlot{};
};

struct WorkspaceSlotCounter {
    std::string workspaceName;
    std::uint64_t nextSlot{};
};

struct DurableProjectionState {
    std::uint32_t schemaVersion{ProjectionStateSchemaVersion};
    InitialPlacementPolicy placementPolicy{};
    std::uint64_t nextWindowId{1};
    std::uint64_t nextWorkspaceRegion{};
    std::vector<PersistedWorkspaceRegion> workspaces;
    std::vector<WorkspaceSlotCounter> workspaceSlots;
    std::vector<PersistedWindowLayout> windows;
};

enum class RestoreMatchKind {
    StrongSameEpochAddress,
    UniqueMetadataFingerprint,
    NoMatch
};

struct RestoreCandidate {
    PersistedWindowLayout layout;
    bool consumed{};
};

class WindowRestorePolicy {
public:
    [[nodiscard]] static bool compatibleStrongIdentity(
        const PersistedWindowLayout& saved,
        const std::string& currentCompositorEpoch,
        const std::string& canonicalAddress,
        const std::optional<std::string>& className,
        const std::optional<std::string>& workspaceName);
    [[nodiscard]] static bool matchesMetadataFingerprint(
        const PersistedWindowLayout& saved,
        const std::optional<std::string>& className,
        const std::optional<std::string>& title,
        const std::optional<std::string>& workspaceName);
};

} // namespace hyprvx::runtime
