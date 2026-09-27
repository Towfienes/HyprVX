#pragma once

#include "hyprvx/core/NavigationState.hpp"
#include "hyprvx/core/ViewportFraming.hpp"
#include "hyprvx/core/World.hpp"
#include "hyprvx/hyprland/RuntimeMirror.hpp"
#include "hyprvx/runtime/ProjectionState.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace hyprvx::runtime {

enum class PlacementStatus {
    Projected,
    DeferredMissingWorkspace
};

struct ProjectedWindowRecord {
    std::string externalAddress;
    WindowId windowId{};
    std::optional<std::string> className;
    std::optional<std::string> title;
    std::optional<int> workspaceId;
    std::optional<std::string> workspaceName;
    std::optional<bool> floating;
    PlacementStatus placementStatus{PlacementStatus::DeferredMissingWorkspace};
    Rect geometry{};
    std::uint64_t placementSlot{};
};

struct WorkspaceRegion {
    std::string name;
    Vec2 origin{};
    Rect bounds{};
    std::optional<int> workspaceId;
};

class WorkspaceSpatialMap {
public:
    explicit WorkspaceSpatialMap(InitialPlacementPolicy policy = {});

    [[nodiscard]] WorkspaceRegion resolve(
        std::string_view workspaceName,
        std::optional<int> workspaceId = std::nullopt);
    void includeRow(std::string_view workspaceName, std::uint64_t row);
    [[nodiscard]] std::uint64_t nextRegionIndex() const noexcept { return nextRegion_; }
    void restore(std::vector<PersistedWorkspaceRegion> regions, std::uint64_t nextRegion);
    [[nodiscard]] const WorkspaceRegion* find(std::string_view workspaceName) const;
    [[nodiscard]] const std::unordered_map<std::string, WorkspaceRegion>& regions() const noexcept {
        return regions_;
    }
    void reset();

private:
    InitialPlacementPolicy policy_;
    std::unordered_map<std::string, WorkspaceRegion> regions_;
    std::uint64_t nextRegion_{};
};

class SpatialProjector {
public:
    explicit SpatialProjector(InitialPlacementPolicy policy = {});

    // Reconciles only observed RuntimeMirror state. Windows without workspace
    // metadata stay deferred until a later event supplies enough placement data.
    void reconcile(const hyprvx::hyprland::RuntimeMirror& mirror, World& world);
    void reset(World& world);
    void endEpoch(World& world);
    void setCompositorEpoch(std::string epoch);
    void restoreState(const DurableProjectionState& state, World& world);
    [[nodiscard]] DurableProjectionState snapshot() const;
    [[nodiscard]] std::uint64_t layoutRevision() const noexcept { return layoutRevision_; }

    [[nodiscard]] const std::unordered_map<std::string, ProjectedWindowRecord>& windows() const noexcept {
        return windows_;
    }
    [[nodiscard]] const ProjectedWindowRecord* findWindow(std::string_view address) const;
    [[nodiscard]] const ProjectedWindowRecord* focusedWindow() const;
    [[nodiscard]] std::optional<WindowId> focusedWindowId() const noexcept;
    [[nodiscard]] std::optional<Rect> geometryForAddress(std::string_view address) const noexcept;
    [[nodiscard]] std::optional<WorkspaceRegion> workspaceRegion(std::string_view name) const;
    [[nodiscard]] std::optional<CameraView> frameFocusedWindow(
        Vec2 viewportPixels,
        double currentZoom,
        ViewportFramingPolicy policy = {}) const;
    [[nodiscard]] std::optional<CameraView> viewForWorkspace(
        std::string_view workspaceName,
        Vec2 viewportPixels,
        double currentZoom,
        ViewportFramingPolicy policy = {}) const;
    [[nodiscard]] std::optional<CameraView> viewForFocusedWindow(
        Vec2 viewportPixels,
        double currentZoom,
        ViewportFramingPolicy policy = {}) const;
    [[nodiscard]] const WorkspaceSpatialMap& workspaceMap() const noexcept { return workspaceMap_; }
    [[nodiscard]] std::optional<WindowId> mappedId(std::string_view address) const;

private:
    [[nodiscard]] WindowId allocateId(const World& world);
    struct Placement { Rect geometry; std::uint64_t slot; };
    [[nodiscard]] Placement allocateGeometry(const std::string& address, const WorkspaceRegion& region);
    void removeProjected(const std::string& address, World& world);
    [[nodiscard]] std::optional<PersistedWindowLayout> takeRestoreCandidate(
        const std::string& address,
        const hyprvx::hyprland::ObservedWindowState& observed,
        const hyprvx::hyprland::RuntimeMirror& mirror,
        const World& world);

    InitialPlacementPolicy policy_;
    WorkspaceSpatialMap workspaceMap_;
    std::unordered_map<std::string, WindowId> addressToId_;
    std::unordered_map<std::string, ProjectedWindowRecord> windows_;
    std::unordered_map<std::string, std::uint64_t> nextSlotByWorkspace_;
    std::vector<PersistedWindowLayout> dormantLayouts_;
    std::unordered_set<WindowId> issuedIds_;
    std::unordered_set<WindowId> candidateIds_;
    std::optional<std::string> focusedAddress_;
    std::string compositorEpoch_;
    WindowId nextId_{1};
    std::uint64_t layoutRevision_{};
};

} // namespace hyprvx::runtime
