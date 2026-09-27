#pragma once

#include "hyprvx/core/NavigationState.hpp"
#include "hyprvx/core/ViewportFraming.hpp"
#include "hyprvx/core/World.hpp"
#include "hyprvx/hyprland/RuntimeMirror.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace hyprvx::runtime {

enum class PlacementStatus {
    Projected,
    DeferredMissingWorkspace
};

struct InitialPlacementPolicy {
    Vec2 windowSize{1000.0, 700.0};
    double workspaceStride{3600.0};
    std::size_t columns{3};
    // Finite initial vertical extent; bounds grow deterministically as rows are used.
    std::size_t initialRegionRows{3};
    double gap{200.0};
    Vec2 firstWorkspaceOrigin{};
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
    [[nodiscard]] Rect allocateGeometry(const std::string& address, const WorkspaceRegion& region);
    void removeProjected(const std::string& address, World& world);

    InitialPlacementPolicy policy_;
    WorkspaceSpatialMap workspaceMap_;
    std::unordered_map<std::string, WindowId> addressToId_;
    std::unordered_map<std::string, ProjectedWindowRecord> windows_;
    std::unordered_map<std::string, std::uint64_t> nextSlotByWorkspace_;
    std::optional<std::string> focusedAddress_;
    WindowId nextId_{1};
};

} // namespace hyprvx::runtime
