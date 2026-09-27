#include "hyprvx/runtime/SpatialProjection.hpp"

#include "hyprvx/core/Camera.hpp"
#include "hyprvx/core/Types.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace hyprvx::runtime {

namespace {

void validatePolicy(const InitialPlacementPolicy& policy) {
    if (!std::isfinite(policy.windowSize.x) || !std::isfinite(policy.windowSize.y) ||
        policy.windowSize.x <= 0.0 || policy.windowSize.y <= 0.0 ||
        !std::isfinite(policy.workspaceStride) || policy.workspaceStride <= 0.0 ||
        policy.columns == 0 || policy.initialRegionRows == 0 ||
        !std::isfinite(policy.gap) || policy.gap < 0.0 ||
        !std::isfinite(policy.firstWorkspaceOrigin.x) || !std::isfinite(policy.firstWorkspaceOrigin.y)) {
        throw std::invalid_argument("invalid initial placement policy");
    }
    const double needed = static_cast<double>(policy.columns - 1) * (policy.windowSize.x + policy.gap) + policy.windowSize.x;
    if (policy.workspaceStride < needed) {
        throw std::invalid_argument("workspace stride is too small for the configured placement columns");
    }
}

std::string canonical(std::string_view address) {
    return hyprvx::hyprland::canonicalWindowAddress(address);
}

double regionHeight(const InitialPlacementPolicy& policy, std::uint64_t rows) {
    const double result = static_cast<double>(rows - 1) * (policy.windowSize.y + policy.gap) + policy.windowSize.y;
    if (!std::isfinite(result) || result <= 0.0) {
        throw std::overflow_error("workspace region height overflow");
    }
    return result;
}

} // namespace

WorkspaceSpatialMap::WorkspaceSpatialMap(InitialPlacementPolicy policy)
    : policy_(policy) {
    validatePolicy(policy_);
}

WorkspaceRegion WorkspaceSpatialMap::resolve(std::string_view workspaceName, std::optional<int> workspaceId) {
    if (workspaceName.empty() ||
        std::any_of(workspaceName.begin(), workspaceName.end(), [](char value) { return value == '\0'; }) ||
        std::all_of(workspaceName.begin(), workspaceName.end(), [](unsigned char value) {
            return std::isspace(value) != 0;
        })) {
        throw std::invalid_argument("workspace name must not be empty or whitespace");
    }
    const std::string key(workspaceName);
    auto found = regions_.find(key);
    if (found != regions_.end()) {
        if (!found->second.workspaceId && workspaceId) {
            found->second.workspaceId = workspaceId;
        }
        return found->second;
    }
    if (nextRegion_ > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        throw std::overflow_error("workspace region index exhausted");
    }
    const double x = policy_.firstWorkspaceOrigin.x + static_cast<double>(nextRegion_) * policy_.workspaceStride;
    if (!std::isfinite(x)) {
        throw std::overflow_error("workspace region coordinate overflow");
    }
    WorkspaceRegion region{
        key,
        {x, policy_.firstWorkspaceOrigin.y},
        {{x, policy_.firstWorkspaceOrigin.y}, {policy_.workspaceStride, regionHeight(policy_, policy_.initialRegionRows)}},
        workspaceId};
    regions_.emplace(key, region);
    ++nextRegion_;
    return region;
}

void WorkspaceSpatialMap::includeRow(std::string_view workspaceName, std::uint64_t row) {
    auto found = regions_.find(std::string(workspaceName));
    if (found == regions_.end()) {
        throw std::invalid_argument("workspace region must be resolved before extending it");
    }
    if (row == std::numeric_limits<std::uint64_t>::max()) {
        throw std::overflow_error("workspace row index exhausted");
    }
    const double height = regionHeight(policy_, row + 1);
    found->second.bounds.size.y = std::max(found->second.bounds.size.y, height);
}

const WorkspaceRegion* WorkspaceSpatialMap::find(std::string_view workspaceName) const {
    const auto found = regions_.find(std::string(workspaceName));
    return found == regions_.end() ? nullptr : &found->second;
}

void WorkspaceSpatialMap::reset() {
    regions_.clear();
    nextRegion_ = 0;
}

SpatialProjector::SpatialProjector(InitialPlacementPolicy policy)
    : policy_(policy), workspaceMap_(policy) {
    validatePolicy(policy_);
}

WindowId SpatialProjector::allocateId(const World& world) {
    while (world.windowExists(nextId_)) {
        if (nextId_ == std::numeric_limits<WindowId>::max()) {
            throw std::overflow_error("window id space exhausted");
        }
        ++nextId_;
    }
    const WindowId result = nextId_;
    if (nextId_ != std::numeric_limits<WindowId>::max()) {
        ++nextId_;
    }
    return result;
}

Rect SpatialProjector::allocateGeometry(const std::string& address, const WorkspaceRegion& region) {
    static_cast<void>(address);
    auto& nextSlot = nextSlotByWorkspace_[region.name];
    const std::uint64_t slot = nextSlot++;
    const std::uint64_t column = slot % static_cast<std::uint64_t>(policy_.columns);
    const std::uint64_t row = slot / static_cast<std::uint64_t>(policy_.columns);
    const double x = region.origin.x + static_cast<double>(column) * (policy_.windowSize.x + policy_.gap);
    const double y = region.origin.y + static_cast<double>(row) * (policy_.windowSize.y + policy_.gap);
    if (!std::isfinite(x) || !std::isfinite(y)) {
        throw std::overflow_error("projected window coordinate overflow");
    }
    workspaceMap_.includeRow(region.name, row);
    return {{x, y}, policy_.windowSize};
}

void SpatialProjector::removeProjected(const std::string& address, World& world) {
    const auto record = windows_.find(address);
    if (record != windows_.end()) {
        if (record->second.placementStatus == PlacementStatus::Projected) {
            world.removeWindow(record->second.windowId);
        }
        windows_.erase(record);
    }
    addressToId_.erase(address);
    if (focusedAddress_ && *focusedAddress_ == address) {
        focusedAddress_.reset();
    }
}

void SpatialProjector::reconcile(const hyprvx::hyprland::RuntimeMirror& mirror, World& world) {
    std::vector<std::string> addresses;
    addresses.reserve(mirror.windows().size());
    for (const auto& [address, state] : mirror.windows()) {
        static_cast<void>(state);
        addresses.push_back(canonical(address));
    }
    std::sort(addresses.begin(), addresses.end());

    std::vector<std::string> stale;
    stale.reserve(windows_.size());
    for (const auto& [address, record] : windows_) {
        static_cast<void>(record);
        if (!mirror.windows().contains(address)) {
            stale.push_back(address);
        }
    }
    for (const auto& address : stale) {
        removeProjected(address, world);
    }

    for (const auto& address : addresses) {
        const auto* observed = mirror.findWindow(address);
        if (observed == nullptr) {
            continue;
        }
        auto idIt = addressToId_.find(address);
        if (idIt == addressToId_.end()) {
            idIt = addressToId_.emplace(address, allocateId(world)).first;
        }
        auto recordIt = windows_.find(address);
        if (!observed->workspaceName || observed->workspaceName->empty()) {
            if (recordIt == windows_.end()) {
                windows_.emplace(address, ProjectedWindowRecord{
                    address, idIt->second, observed->className, observed->title,
                    observed->workspaceId, std::nullopt, observed->floating,
                    PlacementStatus::DeferredMissingWorkspace, {}});
            } else {
                auto& record = recordIt->second;
                record.className = observed->className;
                record.title = observed->title;
                record.workspaceId = observed->workspaceId;
                record.floating = observed->floating;
                record.placementStatus = PlacementStatus::DeferredMissingWorkspace;
                world.removeWindow(record.windowId);
            }
            continue;
        }

        const auto region = workspaceMap_.resolve(*observed->workspaceName, observed->workspaceId);
        if (recordIt == windows_.end()) {
            const Rect geometry = allocateGeometry(address, region);
            const WindowId id = idIt->second;
            if (!world.windowExists(id) && !world.addFreeWindow({id, geometry})) {
                throw std::logic_error("could not add projected free window");
            }
            ProjectedWindowRecord record{
                address, id, observed->className, observed->title,
                observed->workspaceId, observed->workspaceName, observed->floating,
                PlacementStatus::Projected, geometry};
            windows_.emplace(address, std::move(record));
        } else {
            auto& record = recordIt->second;
            if (record.placementStatus == PlacementStatus::DeferredMissingWorkspace && world.windowExists(record.windowId)) {
                record.windowId = allocateId(world);
                idIt->second = record.windowId;
            }
            const bool moved = record.workspaceName != observed->workspaceName;
            record.className = observed->className;
            record.title = observed->title;
            record.workspaceId = observed->workspaceId;
            record.workspaceName = observed->workspaceName;
            record.floating = observed->floating;
            if (record.placementStatus == PlacementStatus::DeferredMissingWorkspace) {
                record.geometry = allocateGeometry(address, region);
                if (!world.windowExists(record.windowId)) {
                    world.addFreeWindow({record.windowId, record.geometry});
                }
                record.placementStatus = PlacementStatus::Projected;
            } else if (moved) {
                record.geometry = allocateGeometry(address, region);
                if (WorldWindow* free = world.freeWindow(record.windowId)) {
                    free->geometry = record.geometry;
                } else if (world.windowExists(record.windowId)) {
                    // Workspace moves follow the projector's explicit relocation policy:
                    // detach from any future grouping and reinsert as a free window.
                    world.removeWindow(record.windowId);
                    if (!world.addFreeWindow({record.windowId, record.geometry})) {
                        throw std::logic_error("could not relocate projected window");
                    }
                } else {
                    if (!world.addFreeWindow({record.windowId, record.geometry})) {
                        throw std::logic_error("could not restore projected free window");
                    }
                }
            } else if (!world.windowExists(record.windowId)) {
                if (!world.addFreeWindow({record.windowId, record.geometry})) {
                    throw std::logic_error("could not restore projected free window");
                }
            }
            record.placementStatus = PlacementStatus::Projected;
        }
    }

    if (mirror.focusedWindow()) {
        const std::string focused = canonical(*mirror.focusedWindow());
        focusedAddress_ = windows_.contains(focused) ? std::optional<std::string>(focused) : std::nullopt;
    } else {
        focusedAddress_.reset();
    }
}

void SpatialProjector::reset(World& world) {
    for (const auto& [address, record] : windows_) {
        static_cast<void>(address);
        if (record.placementStatus == PlacementStatus::Projected) {
            world.removeWindow(record.windowId);
        }
    }
    windows_.clear();
    addressToId_.clear();
    nextSlotByWorkspace_.clear();
    workspaceMap_.reset();
    focusedAddress_.reset();
    nextId_ = 1;
}

const ProjectedWindowRecord* SpatialProjector::findWindow(std::string_view address) const {
    const auto found = windows_.find(canonical(address));
    return found == windows_.end() ? nullptr : &found->second;
}

const ProjectedWindowRecord* SpatialProjector::focusedWindow() const {
    const auto* record = focusedAddress_ ? findWindow(*focusedAddress_) : nullptr;
    return record == nullptr || record->placementStatus != PlacementStatus::Projected ? nullptr : record;
}

std::optional<WindowId> SpatialProjector::focusedWindowId() const noexcept {
    const auto* record = focusedWindow();
    return record == nullptr ? std::nullopt : std::optional<WindowId>(record->windowId);
}

std::optional<Rect> SpatialProjector::geometryForAddress(std::string_view address) const noexcept {
    const auto* record = findWindow(address);
    return record == nullptr || record->placementStatus != PlacementStatus::Projected
        ? std::nullopt : std::optional<Rect>(record->geometry);
}

std::optional<WorkspaceRegion> SpatialProjector::workspaceRegion(std::string_view name) const {
    const auto* region = workspaceMap_.find(name);
    return region == nullptr ? std::nullopt : std::optional<WorkspaceRegion>(*region);
}

std::optional<CameraView> SpatialProjector::frameFocusedWindow(Vec2 viewportPixels, double currentZoom, ViewportFramingPolicy policy) const {
    const auto* record = focusedWindow();
    if (record == nullptr || record->placementStatus != PlacementStatus::Projected) return std::nullopt;
    const FramedView frame = frameRectInViewport(record->geometry, viewportPixels, currentZoom, policy);
    return CameraView{frame.cameraPosition, frame.zoom};
}

std::optional<CameraView> SpatialProjector::viewForWorkspace(std::string_view workspaceName, Vec2 viewportPixels, double currentZoom, ViewportFramingPolicy policy) const {
    const auto* region = workspaceMap_.find(workspaceName);
    if (region == nullptr) return std::nullopt;
    Rect bounds{region->origin, policy_.windowSize};
    for (const auto& [address, record] : windows_) {
        static_cast<void>(address);
        if (record.placementStatus == PlacementStatus::Projected && record.workspaceName == workspaceName) {
            bounds = Rect::united(bounds, record.geometry);
        }
    }
    const FramedView frame = frameRectInViewport(bounds, viewportPixels, currentZoom, policy);
    return CameraView{frame.cameraPosition, frame.zoom};
}

std::optional<CameraView> SpatialProjector::viewForFocusedWindow(Vec2 viewportPixels, double currentZoom, ViewportFramingPolicy policy) const {
    return frameFocusedWindow(viewportPixels, currentZoom, policy);
}

std::optional<WindowId> SpatialProjector::mappedId(std::string_view address) const {
    const auto found = addressToId_.find(canonical(address));
    return found == addressToId_.end() ? std::nullopt : std::optional<WindowId>(found->second);
}

} // namespace hyprvx::runtime
