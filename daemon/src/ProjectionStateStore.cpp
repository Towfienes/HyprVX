#include "hyprvx/runtime/ProjectionStateStore.hpp"

#include "hyprvx/runtime/SpatialProjection.hpp"
#include "hyprvx/hyprland/RuntimeMirror.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <charconv>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>
#include <system_error>
#include <unordered_map>
#include <unistd.h>

namespace hyprvx::runtime {

namespace {

using Json = nlohmann::ordered_json;
std::atomic<std::uint64_t> temporarySequence{};

Json optionalString(const std::optional<std::string>& value) {
    return value ? Json(*value) : Json(nullptr);
}

Json optionalInt(const std::optional<int>& value) {
    return value ? Json(*value) : Json(nullptr);
}

Json optionalBool(const std::optional<bool>& value) {
    return value ? Json(*value) : Json(nullptr);
}

Json vectorJson(Vec2 value) {
    return Json::array({value.x, value.y});
}

Json rectJson(Rect value) {
    return Json::array({value.position.x, value.position.y, value.size.x, value.size.y});
}

Json uint64Json(std::uint64_t value) { return std::to_string(value); }

std::uint64_t readUint64(const Json& value) {
    if (!value.is_string()) throw std::invalid_argument("expected canonical unsigned decimal string");
    const auto text = value.get<std::string>();
    if (text.empty() || (text.size() > 1 && text.front() == '0')) {
        throw std::invalid_argument("unsigned decimal string is not canonical");
    }
    std::uint64_t result{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), result, 10);
    if (error != std::errc{} || end != text.data() + text.size()) {
        throw std::invalid_argument("invalid unsigned decimal string");
    }
    return result;
}

Json encode(const DurableProjectionState& state) {
    Json json;
    json["schema_version"] = state.schemaVersion;
    json["placement_policy"] = {
        {"window_size", vectorJson(state.placementPolicy.windowSize)},
        {"workspace_stride", state.placementPolicy.workspaceStride},
        {"columns", state.placementPolicy.columns},
        {"initial_region_rows", state.placementPolicy.initialRegionRows},
        {"gap", state.placementPolicy.gap},
        {"first_workspace_origin", vectorJson(state.placementPolicy.firstWorkspaceOrigin)}};
    json["next_window_id"] = uint64Json(state.nextWindowId);
    json["next_workspace_region"] = uint64Json(state.nextWorkspaceRegion);
    json["workspaces"] = Json::array();
    for (const auto& workspace : state.workspaces) {
        json["workspaces"].push_back({
            {"name", workspace.name}, {"origin", vectorJson(workspace.origin)},
            {"bounds", rectJson(workspace.bounds)}, {"workspace_id", optionalInt(workspace.workspaceId)}});
    }
    json["workspace_slots"] = Json::array();
    for (const auto& counter : state.workspaceSlots) {
        json["workspace_slots"].push_back({{"workspace", counter.workspaceName}, {"next_slot", uint64Json(counter.nextSlot)}});
    }
    json["windows"] = Json::array();
    for (const auto& window : state.windows) {
        json["windows"].push_back({
            {"address", window.address}, {"compositor_epoch", window.compositorEpoch},
            {"window_id", uint64Json(window.windowId)}, {"class", optionalString(window.className)},
            {"title", optionalString(window.title)}, {"workspace_id", optionalInt(window.workspaceId)},
            {"workspace", optionalString(window.workspaceName)}, {"floating", optionalBool(window.floating)},
            {"geometry", rectJson(window.geometry)}, {"placement_slot", uint64Json(window.placementSlot)}});
    }
    return json;
}

std::optional<std::string> readOptionalString(const Json& value) {
    if (value.is_null()) return std::nullopt;
    if (!value.is_string()) throw std::invalid_argument("expected string or null");
    return value.get<std::string>();
}

std::optional<int> readOptionalInt(const Json& value) {
    if (value.is_null()) return std::nullopt;
    if (!value.is_number_integer()) throw std::invalid_argument("expected integer or null");
    return value.get<int>();
}

std::optional<bool> readOptionalBool(const Json& value) {
    if (value.is_null()) return std::nullopt;
    if (!value.is_boolean()) throw std::invalid_argument("expected boolean or null");
    return value.get<bool>();
}

Vec2 readVec2(const Json& value) {
    if (!value.is_array() || value.size() != 2 || !value[0].is_number() || !value[1].is_number()) {
        throw std::invalid_argument("expected two-coordinate vector");
    }
    return {value[0].get<double>(), value[1].get<double>()};
}

Rect readRect(const Json& value) {
    if (!value.is_array() || value.size() != 4) throw std::invalid_argument("expected four-coordinate rectangle");
    for (const auto& item : value) {
        if (!item.is_number()) throw std::invalid_argument("rectangle coordinates must be numeric");
    }
    return {{value[0].get<double>(), value[1].get<double>()}, {value[2].get<double>(), value[3].get<double>()}};
}

bool validRect(const Rect& rect) {
    const auto bounded = [](double value) {
        return std::isfinite(value) && std::abs(value) <= ProjectionPersistenceWorldLimit;
    };
    return bounded(rect.position.x) && bounded(rect.position.y) &&
        bounded(rect.size.x) && bounded(rect.size.y) && rect.size.x > 0.0 && rect.size.y > 0.0 &&
        bounded(rect.position.x + rect.size.x) && bounded(rect.position.y + rect.size.y);
}

bool validWorkspaceName(const std::string& name) {
    return !name.empty() &&
        std::none_of(name.begin(), name.end(), [](char value) { return value == '\0'; }) &&
        !std::all_of(name.begin(), name.end(), [](unsigned char value) { return std::isspace(value) != 0; });
}

DurableProjectionState decode(const Json& json) {
    if (!json.is_object()) throw std::invalid_argument("state root must be an object");
    if (!json.contains("schema_version") || !json["schema_version"].is_number_unsigned()) {
        throw std::invalid_argument("missing or invalid schema_version");
    }
    const auto version = json["schema_version"].get<std::uint32_t>();
    if (version != ProjectionStateSchemaVersion) throw std::domain_error("unsupported schema version " + std::to_string(version));

    DurableProjectionState state;
    state.schemaVersion = version;
    const auto& placement = json.at("placement_policy");
    state.placementPolicy.windowSize = readVec2(placement.at("window_size"));
    state.placementPolicy.workspaceStride = placement.at("workspace_stride").get<double>();
    state.placementPolicy.columns = placement.at("columns").get<std::size_t>();
    state.placementPolicy.initialRegionRows = placement.at("initial_region_rows").get<std::size_t>();
    state.placementPolicy.gap = placement.at("gap").get<double>();
    state.placementPolicy.firstWorkspaceOrigin = readVec2(placement.at("first_workspace_origin"));
    const auto& policy = state.placementPolicy;
    const double requiredWidth = static_cast<double>(policy.columns == 0 ? 0 : policy.columns - 1) *
        (policy.windowSize.x + policy.gap) + policy.windowSize.x;
    const double initialRegionHeight = static_cast<double>(policy.initialRegionRows == 0 ? 0 : policy.initialRegionRows - 1) *
        (policy.windowSize.y + policy.gap) + policy.windowSize.y;
    if (!std::isfinite(policy.windowSize.x) || !std::isfinite(policy.windowSize.y) ||
        policy.windowSize.x <= 0.0 || policy.windowSize.y <= 0.0 ||
        !std::isfinite(policy.workspaceStride) || policy.workspaceStride <= 0.0 ||
        policy.columns == 0 || policy.initialRegionRows == 0 || !std::isfinite(policy.gap) || policy.gap < 0.0 ||
        !std::isfinite(policy.firstWorkspaceOrigin.x) || !std::isfinite(policy.firstWorkspaceOrigin.y) ||
        std::abs(policy.firstWorkspaceOrigin.x) > ProjectionPersistenceWorldLimit ||
        std::abs(policy.firstWorkspaceOrigin.y) > ProjectionPersistenceWorldLimit ||
        policy.windowSize.x > ProjectionPersistenceWorldLimit || policy.windowSize.y > ProjectionPersistenceWorldLimit ||
        policy.workspaceStride > ProjectionPersistenceWorldLimit || policy.gap > ProjectionPersistenceWorldLimit ||
        !std::isfinite(requiredWidth) || policy.workspaceStride < requiredWidth ||
        !std::isfinite(initialRegionHeight) || initialRegionHeight <= 0.0 || initialRegionHeight > ProjectionPersistenceWorldLimit) {
        throw std::invalid_argument("invalid persisted initial placement policy");
    }
    state.nextWindowId = readUint64(json.at("next_window_id"));
    state.nextWorkspaceRegion = readUint64(json.at("next_workspace_region"));
    if (state.nextWindowId == 0) throw std::invalid_argument("next_window_id must be positive");
    if (!json.at("workspaces").is_array() || !json.at("workspace_slots").is_array() || !json.at("windows").is_array()) {
        throw std::invalid_argument("workspace and window collections must be arrays");
    }

    std::set<std::string> workspaceNames;
    for (const auto& item : json.at("workspaces")) {
        PersistedWorkspaceRegion workspace;
        workspace.name = item.at("name").get<std::string>();
        workspace.origin = readVec2(item.at("origin"));
        workspace.bounds = readRect(item.at("bounds"));
        workspace.workspaceId = readOptionalInt(item.at("workspace_id"));
        if (!validWorkspaceName(workspace.name) || !workspaceNames.insert(workspace.name).second ||
            !std::isfinite(workspace.origin.x) || !std::isfinite(workspace.origin.y) ||
            std::abs(workspace.origin.x) > ProjectionPersistenceWorldLimit ||
            std::abs(workspace.origin.y) > ProjectionPersistenceWorldLimit ||
            workspace.bounds.position.x != workspace.origin.x || workspace.bounds.position.y != workspace.origin.y ||
            workspace.bounds.size.x < policy.workspaceStride ||
            !validRect(workspace.bounds)) {
            throw std::invalid_argument("invalid or duplicate workspace region");
        }
        state.workspaces.push_back(std::move(workspace));
    }
    for (std::size_t left = 0; left < state.workspaces.size(); ++left) {
        for (std::size_t right = left + 1; right < state.workspaces.size(); ++right) {
            const auto& a = state.workspaces[left].bounds;
            const auto& b = state.workspaces[right].bounds;
            if (!(a.right() <= b.left() || b.right() <= a.left())) {
                throw std::invalid_argument("persisted workspace regions overlap");
            }
        }
    }
    if (state.nextWorkspaceRegion < state.workspaces.size()) {
        throw std::invalid_argument("next_workspace_region is inconsistent");
    }

    std::set<std::string> slotNames;
    std::unordered_map<std::string, std::uint64_t> slotCounters;
    for (const auto& item : json.at("workspace_slots")) {
        WorkspaceSlotCounter counter{item.at("workspace").get<std::string>(), readUint64(item.at("next_slot"))};
        if (counter.workspaceName.empty() || !workspaceNames.contains(counter.workspaceName) ||
            !slotNames.insert(counter.workspaceName).second) {
            throw std::invalid_argument("invalid or duplicate workspace slot counter");
        }
        slotCounters.emplace(counter.workspaceName, counter.nextSlot);
        state.workspaceSlots.push_back(std::move(counter));
    }

    std::set<std::pair<std::string, std::string>> identities;
    std::set<WindowId> windowIds;
    WindowId maxId{};
    for (const auto& item : json.at("windows")) {
        PersistedWindowLayout window;
        window.address = item.at("address").get<std::string>();
        window.compositorEpoch = item.at("compositor_epoch").get<std::string>();
        window.windowId = readUint64(item.at("window_id"));
        window.className = readOptionalString(item.at("class"));
        window.title = readOptionalString(item.at("title"));
        window.workspaceId = readOptionalInt(item.at("workspace_id"));
        window.workspaceName = readOptionalString(item.at("workspace"));
        window.floating = readOptionalBool(item.at("floating"));
        window.geometry = readRect(item.at("geometry"));
        window.placementSlot = readUint64(item.at("placement_slot"));
        if (window.address.empty() || hyprvx::hyprland::canonicalWindowAddress(window.address) != window.address ||
            window.compositorEpoch.empty() || window.windowId == 0 ||
            !window.workspaceName || !workspaceNames.contains(*window.workspaceName) || !validRect(window.geometry) ||
            !identities.emplace(window.compositorEpoch, window.address).second || !windowIds.insert(window.windowId).second) {
            throw std::invalid_argument("invalid or duplicate persisted window identity or geometry");
        }
        const auto counter = slotCounters.find(*window.workspaceName);
        if (counter == slotCounters.end() || counter->second <= window.placementSlot) {
            throw std::invalid_argument("persisted placement slot exceeds its workspace counter");
        }
        maxId = std::max(maxId, window.windowId);
        state.windows.push_back(std::move(window));
    }
    if (maxId >= state.nextWindowId && maxId != std::numeric_limits<WindowId>::max()) {
        throw std::invalid_argument("next_window_id is inconsistent");
    }
    return state;
}

struct ParsedFile {
    StateLoadStatus status{StateLoadStatus::Corrupt};
    std::optional<DurableProjectionState> state;
    std::string diagnostic;
};

ParsedFile parseFile(const std::filesystem::path& path) {
    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
        return {error ? StateLoadStatus::IoError : StateLoadStatus::Missing, std::nullopt,
            error ? error.message() : "state file does not exist"};
    }
    try {
        std::ifstream stream(path, std::ios::binary);
        if (!stream) return {StateLoadStatus::IoError, std::nullopt, "could not open state file"};
        Json json;
        stream >> json;
        if (stream.bad()) return {StateLoadStatus::IoError, std::nullopt, "failed reading state file"};
        return {StateLoadStatus::Loaded, decode(json), {}};
    } catch (const std::domain_error& errorValue) {
        return {StateLoadStatus::UnsupportedVersion, std::nullopt, errorValue.what()};
    } catch (const std::exception& errorValue) {
        return {StateLoadStatus::Corrupt, std::nullopt, errorValue.what()};
    }
}

void writeAll(int descriptor, std::string_view data) {
    std::size_t written = 0;
    while (written < data.size()) {
        const ssize_t count = ::write(descriptor, data.data() + written, data.size() - written);
        if (count < 0) {
            if (errno == EINTR) continue;
            throw std::system_error(errno, std::generic_category(), "writing state file");
        }
        if (count == 0) throw std::runtime_error("zero-length write while saving projection state");
        written += static_cast<std::size_t>(count);
    }
}

void atomicWrite(const std::filesystem::path& path, std::string_view contents) {
    const auto parent = path.has_parent_path() ? path.parent_path() : std::filesystem::current_path();
    std::filesystem::create_directories(parent);
    const auto sequence = temporarySequence.fetch_add(1, std::memory_order_relaxed);
    const auto temporary = parent / (path.filename().string() + ".tmp." + std::to_string(::getpid()) + "." + std::to_string(sequence));
    const int descriptor = ::open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (descriptor < 0) throw std::system_error(errno, std::generic_category(), "creating temporary state file");
    bool closed = false;
    try {
        writeAll(descriptor, contents);
        if (::fsync(descriptor) != 0) throw std::system_error(errno, std::generic_category(), "syncing state file");
        if (::close(descriptor) != 0) throw std::system_error(errno, std::generic_category(), "closing state file");
        closed = true;
        if (::rename(temporary.c_str(), path.c_str()) != 0) throw std::system_error(errno, std::generic_category(), "atomically replacing state file");
        const int directory = ::open(parent.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (directory >= 0) {
            static_cast<void>(::fsync(directory));
            static_cast<void>(::close(directory));
        }
    } catch (...) {
        if (!closed) static_cast<void>(::close(descriptor));
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    }
}

std::filesystem::path backupPath(const std::filesystem::path& path) {
    return path.string() + ".bak";
}

std::filesystem::path uniqueCorruptPath(const std::filesystem::path& path) {
    const auto first = std::filesystem::path(path.string() + ".corrupt");
    if (!std::filesystem::exists(first)) return first;
    for (std::uint64_t suffix = 1; suffix != std::numeric_limits<std::uint64_t>::max(); ++suffix) {
        const auto candidate = std::filesystem::path(first.string() + "." + std::to_string(suffix));
        if (!std::filesystem::exists(candidate)) return candidate;
    }
    throw std::runtime_error("corrupt-state diagnostic name space exhausted");
}

} // namespace

std::filesystem::path ProjectionStateStore::defaultPath() {
    if (const char* stateHome = std::getenv("XDG_STATE_HOME"); stateHome != nullptr && *stateHome != '\0') {
        return std::filesystem::path(stateHome) / "hyprvx" / "state.json";
    }
    if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0') {
        return std::filesystem::path(home) / ".local" / "state" / "hyprvx" / "state.json";
    }
    throw std::runtime_error("neither XDG_STATE_HOME nor HOME is set");
}

StateLoadResult ProjectionStateStore::load(const std::filesystem::path& path) const {
    const ParsedFile primary = parseFile(path);
    if (primary.status == StateLoadStatus::Loaded) {
        return {StateLoadStatus::Loaded, primary.state, {}};
    }
    if (primary.status == StateLoadStatus::UnsupportedVersion) {
        return {primary.status, std::nullopt, primary.diagnostic};
    }
    const ParsedFile backup = parseFile(backupPath(path));
    if (backup.status == StateLoadStatus::Loaded) {
        return {StateLoadStatus::RecoveredBackup, backup.state,
            "primary state unavailable (" + primary.diagnostic + "); loaded last-good backup"};
    }
    if (primary.status == StateLoadStatus::Missing && backup.status == StateLoadStatus::Missing) {
        return {StateLoadStatus::Missing, std::nullopt, primary.diagnostic};
    }
    if (primary.status == StateLoadStatus::IoError) {
        return {StateLoadStatus::IoError, std::nullopt, primary.diagnostic};
    }
    return {backup.status == StateLoadStatus::UnsupportedVersion ? backup.status : StateLoadStatus::Corrupt,
        std::nullopt, "primary: " + primary.diagnostic + "; backup: " + backup.diagnostic};
}

void ProjectionStateStore::saveAtomic(
    const std::filesystem::path& path,
    const DurableProjectionState& state,
    bool replaceInvalidExisting) const {
    const auto validated = decode(encode(state));
    const std::string contents = encode(validated).dump(2) + "\n";
    const ParsedFile existing = parseFile(path);
    if (existing.status == StateLoadStatus::Loaded) {
        std::ifstream input(path, std::ios::binary);
        const std::string oldContents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        atomicWrite(backupPath(path), oldContents);
    } else if (existing.status != StateLoadStatus::Missing) {
        if (!replaceInvalidExisting) {
            throw std::runtime_error("refusing to replace invalid state file without explicit approval: " + existing.diagnostic);
        }
        const auto diagnosticPath = uniqueCorruptPath(path);
        std::filesystem::copy_file(path, diagnosticPath);
    }
    atomicWrite(path, contents);
}

ProjectionStateManager::ProjectionStateManager(
    std::filesystem::path path,
    std::chrono::milliseconds debounce,
    std::chrono::milliseconds maxDirtyDelay)
    : path_(std::move(path)), debounce_(debounce), maxDirtyDelay_(maxDirtyDelay) {
    if (path_.empty() || debounce_.count() < 0 || maxDirtyDelay_.count() < 0) {
        throw std::invalid_argument("invalid projection state manager configuration");
    }
}

StateLoadResult ProjectionStateManager::load() const {
    return store_.load(path_);
}

void ProjectionStateManager::observeLayout(const SpatialProjector& projector, Clock::time_point now) {
    const auto revision = projector.layoutRevision();
    if (revision != observedRevision_) {
        observedRevision_ = revision;
        dirty_ = true;
        if (!firstDirtyAt_) firstDirtyAt_ = now;
        deadline_ = std::min(now + debounce_, *firstDirtyAt_ + maxDirtyDelay_);
    }
}

bool ProjectionStateManager::flushIfDue(
    Clock::time_point now,
    const SpatialProjector& projector,
    bool replaceInvalidExisting) {
    if (!dirty_ || !deadline_ || now < *deadline_) return false;
    saveNow(projector, replaceInvalidExisting);
    return true;
}

void ProjectionStateManager::saveNow(const SpatialProjector& projector, bool replaceInvalidExisting) {
    store_.saveAtomic(path_, projector.snapshot(), replaceInvalidExisting);
    observedRevision_ = projector.layoutRevision();
    dirty_ = false;
    deadline_.reset();
    firstDirtyAt_.reset();
}

} // namespace hyprvx::runtime
