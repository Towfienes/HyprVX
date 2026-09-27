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
        !std::isfinite(policy.firstWorkspaceOrigin.x) || !std::isfinite(policy.firstWorkspaceOrigin.y) ||
        std::abs(policy.windowSize.x) > ProjectionPersistenceWorldLimit ||
        std::abs(policy.windowSize.y) > ProjectionPersistenceWorldLimit ||
        std::abs(policy.workspaceStride) > ProjectionPersistenceWorldLimit ||
        std::abs(policy.gap) > ProjectionPersistenceWorldLimit ||
        std::abs(policy.firstWorkspaceOrigin.x) > ProjectionPersistenceWorldLimit ||
        std::abs(policy.firstWorkspaceOrigin.y) > ProjectionPersistenceWorldLimit) {
        throw std::invalid_argument("invalid initial placement policy");
    }
    const double needed = static_cast<double>(policy.columns - 1) * (policy.windowSize.x + policy.gap) + policy.windowSize.x;
    const double initialHeight = static_cast<double>(policy.initialRegionRows - 1) * (policy.windowSize.y + policy.gap) + policy.windowSize.y;
    if (!std::isfinite(needed) || !std::isfinite(initialHeight) || policy.workspaceStride < needed ||
        initialHeight <= 0.0 || initialHeight > ProjectionPersistenceWorldLimit) {
        throw std::invalid_argument("workspace stride is too small for the configured placement columns");
    }
}

std::string canonical(std::string_view address) {
    return hyprvx::hyprland::canonicalWindowAddress(address);
}

double regionHeight(const InitialPlacementPolicy& policy, std::uint64_t rows) {
    const double result = static_cast<double>(rows - 1) * (policy.windowSize.y + policy.gap) + policy.windowSize.y;
    if (!std::isfinite(result) || result <= 0.0 || result > ProjectionPersistenceWorldLimit) {
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
    if (!std::isfinite(x) || std::abs(x) > ProjectionPersistenceWorldLimit) {
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

void WorkspaceSpatialMap::restore(std::vector<PersistedWorkspaceRegion> regions, std::uint64_t nextRegion) {
    std::unordered_map<std::string, WorkspaceRegion> restored;
    restored.reserve(regions.size());
    for (auto& region : regions) {
        if (region.name.empty() ||
            std::any_of(region.name.begin(), region.name.end(), [](char value) { return value == '\0'; }) ||
            std::all_of(region.name.begin(), region.name.end(), [](unsigned char value) { return std::isspace(value) != 0; }) ||
            !std::isfinite(region.origin.x) || !std::isfinite(region.origin.y) ||
            !std::isfinite(region.bounds.position.x) || !std::isfinite(region.bounds.position.y) ||
            !std::isfinite(region.bounds.size.x) || !std::isfinite(region.bounds.size.y) ||
            region.bounds.size.x <= 0.0 || region.bounds.size.y <= 0.0 ||
            region.bounds.position.x != region.origin.x || region.bounds.position.y != region.origin.y ||
            region.bounds.size.x < policy_.workspaceStride) {
            throw std::invalid_argument("invalid persisted workspace region");
        }
        const std::string name = region.name;
        if (!restored.emplace(name, WorkspaceRegion{
                std::move(region.name), region.origin, region.bounds, region.workspaceId}).second) {
            throw std::invalid_argument("duplicate persisted workspace region");
        }
    }
    if (nextRegion < restored.size()) {
        throw std::invalid_argument("persisted workspace region counter is inconsistent");
    }
    regions_ = std::move(restored);
    nextRegion_ = nextRegion;
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
    while (world.windowExists(nextId_) || issuedIds_.contains(nextId_) || candidateIds_.contains(nextId_)) {
        if (nextId_ == std::numeric_limits<WindowId>::max()) {
            throw std::overflow_error("window id space exhausted");
        }
        ++nextId_;
    }
    const WindowId result = nextId_;
    issuedIds_.insert(result);
    if (nextId_ != std::numeric_limits<WindowId>::max()) {
        ++nextId_;
    }
    return result;
}

SpatialProjector::Placement SpatialProjector::allocateGeometry(const std::string& address, const WorkspaceRegion& region) {
    static_cast<void>(address);
    auto& nextSlot = nextSlotByWorkspace_[region.name];
    const std::uint64_t slot = nextSlot++;
    const std::uint64_t column = slot % static_cast<std::uint64_t>(policy_.columns);
    const std::uint64_t row = slot / static_cast<std::uint64_t>(policy_.columns);
    const double x = region.origin.x + static_cast<double>(column) * (policy_.windowSize.x + policy_.gap);
    const double y = region.origin.y + static_cast<double>(row) * (policy_.windowSize.y + policy_.gap);
    if (!std::isfinite(x) || !std::isfinite(y) || std::abs(x) > ProjectionPersistenceWorldLimit ||
        std::abs(y) > ProjectionPersistenceWorldLimit ||
        std::abs(x + policy_.windowSize.x) > ProjectionPersistenceWorldLimit ||
        std::abs(y + policy_.windowSize.y) > ProjectionPersistenceWorldLimit) {
        throw std::overflow_error("projected window coordinate overflow");
    }
    workspaceMap_.includeRow(region.name, row);
    return {{{x, y}, policy_.windowSize}, slot};
}

bool WindowRestorePolicy::compatibleStrongIdentity(
    const PersistedWindowLayout& saved,
    const std::string& currentCompositorEpoch,
    const std::string& canonicalAddress,
    const std::optional<std::string>& className,
    const std::optional<std::string>& workspaceName) {
    if (currentCompositorEpoch.empty() || saved.compositorEpoch != currentCompositorEpoch || saved.address != canonicalAddress) {
        return false;
    }
    if (saved.className && className && saved.className != className) {
        return false;
    }
    if (saved.workspaceName && workspaceName && saved.workspaceName != workspaceName) {
        return false;
    }
    return true;
}

bool WindowRestorePolicy::matchesMetadataFingerprint(
    const PersistedWindowLayout& saved,
    const std::optional<std::string>& className,
    const std::optional<std::string>& title,
    const std::optional<std::string>& workspaceName) {
    return className && title && workspaceName && !className->empty() &&
        saved.className == className && saved.title == title && saved.workspaceName == workspaceName;
}

void SpatialProjector::setCompositorEpoch(std::string epoch) {
    if (!windows_.empty() && compositorEpoch_ != epoch) {
        throw std::logic_error("end the active projector epoch before changing compositor identity");
    }
    compositorEpoch_ = std::move(epoch);
}

std::optional<PersistedWindowLayout> SpatialProjector::takeRestoreCandidate(
    const std::string& address,
    const hyprvx::hyprland::ObservedWindowState& observed,
    const hyprvx::hyprland::RuntimeMirror& mirror,
    const World& world) {
    const auto candidate = std::find_if(dormantLayouts_.begin(), dormantLayouts_.end(), [&](const auto& saved) {
        return candidateIds_.contains(saved.windowId) && !issuedIds_.contains(saved.windowId) && !world.windowExists(saved.windowId) &&
            !observed.openedInCurrentEpoch &&
            WindowRestorePolicy::compatibleStrongIdentity(saved, compositorEpoch_, address,
                observed.className, observed.workspaceName);
    });
    if (candidate != dormantLayouts_.end()) {
        PersistedWindowLayout restored = *candidate;
        dormantLayouts_.erase(candidate);
        candidateIds_.erase(restored.windowId);
        issuedIds_.insert(restored.windowId);
        return restored;
    }

    if (observed.openedInCurrentEpoch) {
        for (auto iterator = dormantLayouts_.begin(); iterator != dormantLayouts_.end();) {
            if (iterator->address == address && iterator->compositorEpoch == compositorEpoch_) {
                candidateIds_.erase(iterator->windowId);
                iterator = dormantLayouts_.erase(iterator);
            } else {
                ++iterator;
            }
        }
    }

    if (!observed.className || !observed.title || !observed.workspaceName || observed.className->empty()) return std::nullopt;
    std::vector<std::size_t> savedMatches;
    for (std::size_t index = 0; index < dormantLayouts_.size(); ++index) {
        const auto& saved = dormantLayouts_[index];
        if (saved.address == address && saved.compositorEpoch == compositorEpoch_ && observed.openedInCurrentEpoch) {
            continue;
        }
        if (WindowRestorePolicy::matchesMetadataFingerprint(saved, observed.className, observed.title, observed.workspaceName) && candidateIds_.contains(saved.windowId) &&
            !issuedIds_.contains(saved.windowId) && !world.windowExists(saved.windowId)) {
            savedMatches.push_back(index);
        }
    }
    if (savedMatches.size() != 1) {
        return std::nullopt;
    }
    std::size_t liveMatches = 0;
    for (const auto& [liveAddress, live] : mirror.windows()) {
        static_cast<void>(liveAddress);
        if (live.className == observed.className && live.title == observed.title && live.workspaceName == observed.workspaceName) {
            ++liveMatches;
        }
    }
    if (liveMatches != 1) {
        return std::nullopt;
    }
    PersistedWindowLayout restored = dormantLayouts_[savedMatches.front()];
    dormantLayouts_.erase(dormantLayouts_.begin() + static_cast<std::ptrdiff_t>(savedMatches.front()));
    candidateIds_.erase(restored.windowId);
    issuedIds_.insert(restored.windowId);
    return restored;
}

void SpatialProjector::removeProjected(const std::string& address, World& world) {
    const auto record = windows_.find(address);
    if (record != windows_.end()) {
        if (record->second.placementStatus == PlacementStatus::Projected) {
            const auto& value = record->second;
            for (auto iterator = dormantLayouts_.begin(); iterator != dormantLayouts_.end();) {
                if (iterator->address == address && iterator->compositorEpoch == compositorEpoch_) {
                    candidateIds_.erase(iterator->windowId);
                    iterator = dormantLayouts_.erase(iterator);
                } else {
                    ++iterator;
                }
            }
            dormantLayouts_.push_back({
                address, compositorEpoch_, value.windowId, value.className, value.title,
                value.workspaceId, value.workspaceName, value.floating, value.geometry, value.placementSlot});
            candidateIds_.insert(value.windowId);
            world.removeWindow(record->second.windowId);
            issuedIds_.erase(record->second.windowId);
            ++layoutRevision_;
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
        auto recordIt = windows_.find(address);
        if (!observed->workspaceName || observed->workspaceName->empty()) {
            if (recordIt == windows_.end()) {
                if (idIt == addressToId_.end()) {
                    idIt = addressToId_.emplace(address, allocateId(world)).first;
                }
                windows_.emplace(address, ProjectedWindowRecord{
                    address, idIt->second, observed->className, observed->title,
                    observed->workspaceId, std::nullopt, observed->floating,
                    PlacementStatus::DeferredMissingWorkspace, {}, 0});
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
            auto restored = takeRestoreCandidate(address, *observed, mirror, world);
            Placement placement{};
            WindowId id{};
            if (restored) {
                if (idIt != addressToId_.end()) {
                    issuedIds_.erase(idIt->second);
                    addressToId_.erase(idIt);
                }
                id = restored->windowId;
                placement = {restored->geometry, restored->placementSlot};
                nextId_ = std::max(nextId_, id == std::numeric_limits<WindowId>::max() ? id : id + 1);
            } else {
                if (idIt == addressToId_.end()) {
                    idIt = addressToId_.emplace(address, allocateId(world)).first;
                }
                id = idIt->second;
                placement = allocateGeometry(address, region);
                ++layoutRevision_;
            }
            addressToId_[address] = id;
            if (!world.windowExists(id) && !world.addFreeWindow({id, placement.geometry})) {
                throw std::logic_error("could not add projected free window");
            }
            ProjectedWindowRecord record{
                address, id,
                restored && !observed->className ? restored->className : observed->className,
                restored && !observed->title ? restored->title : observed->title,
                restored && !observed->workspaceId ? restored->workspaceId : observed->workspaceId,
                observed->workspaceName,
                restored && !observed->floating ? restored->floating : observed->floating,
                PlacementStatus::Projected, placement.geometry, placement.slot};
            windows_.emplace(address, std::move(record));
        } else {
            auto& record = recordIt->second;
            if (idIt == addressToId_.end()) {
                idIt = addressToId_.emplace(address, record.windowId).first;
            }
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
                auto restored = takeRestoreCandidate(address, *observed, mirror, world);
                if (restored) {
                    issuedIds_.erase(record.windowId);
                    record.windowId = restored->windowId;
                    idIt->second = record.windowId;
                    record.geometry = restored->geometry;
                    record.placementSlot = restored->placementSlot;
                    nextId_ = std::max(nextId_, record.windowId == std::numeric_limits<WindowId>::max()
                        ? record.windowId : record.windowId + 1);
                    if (!observed->className) record.className = restored->className;
                    if (!observed->title) record.title = restored->title;
                    if (!observed->workspaceId) record.workspaceId = restored->workspaceId;
                    if (!observed->floating) record.floating = restored->floating;
                } else {
                    const auto placement = allocateGeometry(address, region);
                    record.geometry = placement.geometry;
                    record.placementSlot = placement.slot;
                    ++layoutRevision_;
                }
                if (!world.windowExists(record.windowId)) {
                    if (!world.addFreeWindow({record.windowId, record.geometry})) {
                        throw std::logic_error("could not add deferred projected window");
                    }
                }
                record.placementStatus = PlacementStatus::Projected;
            } else if (moved) {
                const auto placement = allocateGeometry(address, region);
                record.geometry = placement.geometry;
                record.placementSlot = placement.slot;
                ++layoutRevision_;
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
    dormantLayouts_.clear();
    issuedIds_.clear();
    candidateIds_.clear();
    workspaceMap_.reset();
    focusedAddress_.reset();
    nextId_ = 1;
    layoutRevision_ = 0;
}

void SpatialProjector::endEpoch(World& world) {
    const auto revision = layoutRevision_;
    std::vector<std::string> addresses;
    addresses.reserve(windows_.size());
    for (const auto& [address, record] : windows_) {
        if (record.placementStatus == PlacementStatus::Projected) {
            addresses.push_back(address);
        }
    }
    for (const auto& address : addresses) {
        removeProjected(address, world);
    }
    windows_.clear();
    addressToId_.clear();
    focusedAddress_.reset();
    layoutRevision_ = revision;
}

void SpatialProjector::restoreState(const DurableProjectionState& state, World& world) {
    if (state.schemaVersion != ProjectionStateSchemaVersion) {
        throw std::invalid_argument("unsupported projection state schema version");
    }
    reset(world);
    std::unordered_map<std::string, std::uint64_t> slotCounters;
    for (const auto& counter : state.workspaceSlots) {
        if (counter.workspaceName.empty() || !slotCounters.emplace(counter.workspaceName, counter.nextSlot).second) {
            throw std::invalid_argument("invalid persisted workspace slot counter");
        }
    }
    WorkspaceSpatialMap restoredWorkspaceMap(state.placementPolicy);
    policy_ = state.placementPolicy;
    workspaceMap_ = std::move(restoredWorkspaceMap);
    std::vector<PersistedWorkspaceRegion> regions = state.workspaces;
    workspaceMap_.restore(std::move(regions), state.nextWorkspaceRegion);
    for (const auto& counter : state.workspaceSlots) {
        if (workspaceMap_.find(counter.workspaceName) == nullptr ||
            !nextSlotByWorkspace_.emplace(counter.workspaceName, counter.nextSlot).second) {
            throw std::invalid_argument("invalid persisted workspace slot counter");
        }
    }
    std::unordered_set<WindowId> restoredIds;
    std::unordered_set<std::string> strongKeys;
    WindowId maxId{};
    for (const auto& layout : state.windows) {
        const std::string address = canonical(layout.address);
        if (address.empty() || address != layout.address || layout.windowId == 0 || layout.compositorEpoch.empty() ||
            !std::isfinite(layout.geometry.position.x) || !std::isfinite(layout.geometry.position.y) ||
            !std::isfinite(layout.geometry.size.x) || !std::isfinite(layout.geometry.size.y) ||
            layout.geometry.size.x <= 0.0 || layout.geometry.size.y <= 0.0 ||
            !layout.workspaceName || layout.workspaceName->empty() || workspaceMap_.find(*layout.workspaceName) == nullptr ||
            !restoredIds.insert(layout.windowId).second) {
            throw std::invalid_argument("invalid or duplicate persisted window layout");
        }
        const auto slot = slotCounters.find(*layout.workspaceName);
        if (slot == slotCounters.end() || layout.placementSlot >= slot->second) {
            throw std::invalid_argument("persisted window placement slot is inconsistent with workspace counter");
        }
        const auto& geometry = layout.geometry;
        const auto withinLimit = [](double value) {
            return std::isfinite(value) && std::abs(value) <= ProjectionPersistenceWorldLimit;
        };
        if (!withinLimit(geometry.position.x) || !withinLimit(geometry.position.y) ||
            !withinLimit(geometry.size.x) || !withinLimit(geometry.size.y) ||
            geometry.size.x <= 0.0 || geometry.size.y <= 0.0 ||
            !withinLimit(geometry.position.x + geometry.size.x) ||
            !withinLimit(geometry.position.y + geometry.size.y)) {
            throw std::invalid_argument("persisted window geometry exceeds supported bounds");
        }
        const std::string key = layout.compositorEpoch + "\n" + address;
        if (!strongKeys.insert(key).second) {
            throw std::invalid_argument("duplicate persisted window identity");
        }
        dormantLayouts_.push_back(layout);
        candidateIds_.insert(layout.windowId);
        maxId = std::max(maxId, layout.windowId);
    }
    if (maxId == std::numeric_limits<WindowId>::max()) {
        nextId_ = maxId;
    } else {
        nextId_ = std::max(state.nextWindowId, maxId + 1);
    }
    layoutRevision_ = 0;
}

DurableProjectionState SpatialProjector::snapshot() const {
    DurableProjectionState state;
    state.placementPolicy = policy_;
    state.nextWindowId = nextId_;
    state.nextWorkspaceRegion = workspaceMap_.nextRegionIndex();
    state.workspaces.reserve(workspaceMap_.regions().size());
    for (const auto& [name, region] : workspaceMap_.regions()) {
        static_cast<void>(name);
        state.workspaces.push_back({region.name, region.origin, region.bounds, region.workspaceId});
    }
    std::sort(state.workspaces.begin(), state.workspaces.end(), [](const auto& a, const auto& b) { return a.name < b.name; });
    state.workspaceSlots.reserve(nextSlotByWorkspace_.size());
    for (const auto& [name, next] : nextSlotByWorkspace_) {
        state.workspaceSlots.push_back({name, next});
    }
    std::sort(state.workspaceSlots.begin(), state.workspaceSlots.end(), [](const auto& a, const auto& b) {
        return a.workspaceName < b.workspaceName;
    });
    state.windows = dormantLayouts_;
    for (const auto& [address, record] : windows_) {
        if (record.placementStatus != PlacementStatus::Projected) {
            continue;
        }
        state.windows.push_back({address, compositorEpoch_, record.windowId, record.className, record.title,
            record.workspaceId, record.workspaceName, record.floating, record.geometry, record.placementSlot});
    }
    std::sort(state.windows.begin(), state.windows.end(), [](const auto& a, const auto& b) {
        return a.compositorEpoch == b.compositorEpoch ? a.address < b.address : a.compositorEpoch < b.compositorEpoch;
    });
    return state;
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
