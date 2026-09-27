#include "hyprvx/hyprland/RuntimeBridge.hpp"
#include "hyprvx/runtime/ProjectionStateStore.hpp"
#include "hyprvx/runtime/SpatialProjection.hpp"

#include <gtest/gtest.h>

#include <array>
#include <charconv>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <string>
#include <unistd.h>

using namespace hyprvx;
using namespace hyprvx::hyprland;
using namespace hyprvx::runtime;

namespace {

class TempDirectory {
public:
    TempDirectory() {
        static std::atomic<unsigned> sequence{};
        path_ = std::filesystem::temp_directory_path() /
            ("hyprvx-state-test-" + std::to_string(::getpid()) + "-" + std::to_string(sequence++));
        std::filesystem::create_directories(path_);
    }
    ~TempDirectory() { std::error_code ignored; std::filesystem::remove_all(path_, ignored); }
    const std::filesystem::path& path() const { return path_; }
private:
    std::filesystem::path path_;
};

DurableProjectionState sampleState() {
    DurableProjectionState state;
    state.nextWindowId = 2;
    state.nextWorkspaceRegion = 1;
    state.workspaces.push_back({"1", {0.0, 0.0}, {{0.0, 0.0}, {3600.0, 2500.0}}, 1});
    state.workspaceSlots.push_back({"1", 1});
    state.windows.push_back({"abc", "compositor-a", 1, "foot", "terminal", 1, "1", false,
        {{0.0, 0.0}, {1000.0, 700.0}}, 0});
    return state;
}

void replayLines(RuntimeBridge& bridge, SpatialProjector& projector, World& world,
    const std::initializer_list<std::string_view>& lines) {
    for (const auto line : lines) {
        static_cast<void>(bridge.processLine(line));
        projector.reconcile(bridge.mirror(), world);
    }
}

} // namespace

TEST(ProjectionStateStoreTest, SaveLoadRoundTrip) {
    TempDirectory directory;
    ProjectionStateStore store;
    const auto path = directory.path() / "state.json";
    store.saveAtomic(path, sampleState());
    const auto loaded = store.load(path);
    ASSERT_EQ(loaded.status, StateLoadStatus::Loaded);
    ASSERT_TRUE(loaded.state);
    EXPECT_EQ(loaded.state->schemaVersion, ProjectionStateSchemaVersion);
    ASSERT_EQ(loaded.state->windows.size(), 1U);
    EXPECT_EQ(loaded.state->windows.front().geometry.position.x, 0.0);
    EXPECT_EQ(loaded.state->windows.front().workspaceName, "1");
}

TEST(ProjectionStateStoreTest, Unsigned64FieldsAreCanonicalStringsAndRoundTripFullRange) {
    TempDirectory directory;
    ProjectionStateStore store;
    for (const auto value : {std::uint64_t{9007199254740993ULL}, std::uint64_t{(1ULL << 55) + 3},
             std::uint64_t{std::numeric_limits<std::uint64_t>::max() - 2}}) {
        auto state = sampleState();
        state.nextWindowId = std::numeric_limits<std::uint64_t>::max();
        state.workspaces[0].bounds = {{0.0, 0.0}, {3600.0, 2500.0}};
        state.windows[0].windowId = value;
        state.windows[0].placementSlot = value;
        state.workspaceSlots[0].nextSlot = value + 1;
        const auto path = directory.path() / (std::to_string(value) + ".json");
        store.saveAtomic(path, state);
        std::ifstream input(path);
        const auto json = nlohmann::json::parse(input);
        EXPECT_TRUE(json["next_window_id"].is_string());
        EXPECT_TRUE(json["windows"][0]["window_id"].is_string());
        EXPECT_TRUE(json["windows"][0]["placement_slot"].is_string());
        EXPECT_TRUE(json["workspace_slots"][0]["next_slot"].is_string());
        const auto loaded = store.load(path);
        ASSERT_EQ(loaded.status, StateLoadStatus::Loaded);
        ASSERT_TRUE(loaded.state);
        EXPECT_EQ(loaded.state->windows[0].windowId, value);
        EXPECT_EQ(loaded.state->windows[0].placementSlot, value);
        EXPECT_EQ(loaded.state->workspaceSlots[0].nextSlot, value + 1);
    }
}

TEST(ProjectionStateStoreTest, MaximumUnsignedWindowIdRoundTripsExactly) {
    TempDirectory directory;
    ProjectionStateStore store;
    auto state = sampleState();
    state.windows[0].windowId = std::numeric_limits<std::uint64_t>::max();
    state.nextWindowId = std::numeric_limits<std::uint64_t>::max();
    state.windows[0].placementSlot = std::numeric_limits<std::uint64_t>::max() - 1;
    state.workspaceSlots[0].nextSlot = std::numeric_limits<std::uint64_t>::max();
    const auto path = directory.path() / "max.json";
    store.saveAtomic(path, state);
    const auto loaded = store.load(path);
    ASSERT_EQ(loaded.status, StateLoadStatus::Loaded);
    ASSERT_TRUE(loaded.state);
    EXPECT_EQ(loaded.state->windows[0].windowId, std::numeric_limits<std::uint64_t>::max());
    std::ifstream input(path);
    const auto json = nlohmann::json::parse(input);
    EXPECT_EQ(json["windows"][0]["window_id"], "18446744073709551615");
    EXPECT_EQ(json["next_window_id"], "18446744073709551615");
    EXPECT_EQ(json["workspace_slots"][0]["next_slot"], "18446744073709551615");
}

TEST(ProjectionStateStoreTest, UnsignedFieldsRejectFractionsScientificNotationOverflowAndMalformedStrings) {
    TempDirectory directory;
    ProjectionStateStore store;
    const auto path = directory.path() / "state.json";
    store.saveAtomic(path, sampleState());
    std::ifstream source(path);
    const auto baseline = nlohmann::json::parse(source);
    for (const auto& invalid : {nlohmann::json(1.5), nlohmann::json::parse("1e3"),
             nlohmann::json("18446744073709551616"), nlohmann::json("-1"), nlohmann::json("12x"),
             nlohmann::json("+1"), nlohmann::json(" 1"), nlohmann::json("01")}) {
        auto json = baseline;
        json["windows"][0]["window_id"] = invalid;
        { std::ofstream output(path); output << json.dump(); }
        EXPECT_EQ(store.load(path).status, StateLoadStatus::Corrupt);
    }
}

TEST(ProjectionStateStoreTest, GeometryIsBoundedForPersistence) {
    TempDirectory directory;
    ProjectionStateStore store;
    const auto path = directory.path() / "state.json";
    auto state = sampleState();
    state.windows[0].geometry.position = {-ProjectionPersistenceWorldLimit, -ProjectionPersistenceWorldLimit};
    state.windows[0].geometry.size = {1.0, 1.0};
    store.saveAtomic(path, state);
    EXPECT_EQ(store.load(path).status, StateLoadStatus::Loaded);

    state.windows[0].geometry.position = {ProjectionPersistenceWorldLimit, 0.0};
    EXPECT_THROW(store.saveAtomic(path, state), std::invalid_argument);
    state = sampleState();
    state.windows[0].geometry.position.x = ProjectionPersistenceWorldLimit + 1.0;
    EXPECT_THROW(store.saveAtomic(path, state), std::invalid_argument);
    state = sampleState();
    state.windows[0].geometry.position.x = 1.0e300;
    EXPECT_THROW(store.saveAtomic(path, state), std::invalid_argument);
    state = sampleState();
    state.windows[0].geometry.position.x = ProjectionPersistenceWorldLimit - 10.0;
    state.windows[0].geometry.size.x = 20.0;
    EXPECT_THROW(store.saveAtomic(path, state), std::invalid_argument);
}

TEST(ProjectionStateStoreTest, SerializationIsDeterministic) {
    TempDirectory directory;
    ProjectionStateStore store;
    const auto first = directory.path() / "first.json";
    const auto second = directory.path() / "second.json";
    auto state = sampleState();
    state.workspaces.push_back({"special:chat", {3600.0, 0.0}, {{3600.0, 0.0}, {3600.0, 2500.0}}, std::nullopt});
    state.nextWorkspaceRegion = 2;
    store.saveAtomic(first, state);
    store.saveAtomic(second, state);
    std::ifstream a(first, std::ios::binary), b(second, std::ios::binary);
    const std::string aText((std::istreambuf_iterator<char>(a)), {});
    const std::string bText((std::istreambuf_iterator<char>(b)), {});
    EXPECT_EQ(aText, bText);
}

TEST(ProjectionStateStoreTest, MissingFileReturnsMissingWithoutCreatingIt) {
    TempDirectory directory;
    ProjectionStateStore store;
    const auto path = directory.path() / "missing.json";
    EXPECT_EQ(store.load(path).status, StateLoadStatus::Missing);
    EXPECT_FALSE(std::filesystem::exists(path));
}

TEST(ProjectionStateStoreTest, StaleTemporaryFileIsIgnoredWhenCanonicalStateIsMissing) {
    TempDirectory directory;
    ProjectionStateStore store;
    const auto path = directory.path() / "state.json";
    { std::ofstream output(path.string() + ".tmp.old"); output << "partial"; }
    EXPECT_EQ(store.load(path).status, StateLoadStatus::Missing);
}

TEST(ProjectionStateStoreTest, EmptyAndTruncatedFilesAreCorrupt) {
    TempDirectory directory;
    ProjectionStateStore store;
    const auto path = directory.path() / "state.json";
    { std::ofstream output(path); }
    EXPECT_EQ(store.load(path).status, StateLoadStatus::Corrupt);
    { std::ofstream output(path); output << "{\"schema_version\":1,"; }
    EXPECT_EQ(store.load(path).status, StateLoadStatus::Corrupt);
}

TEST(ProjectionStateStoreTest, UnsupportedSchemaVersionIsRejectedWithoutBackupDowngrade) {
    TempDirectory directory;
    ProjectionStateStore store;
    const auto path = directory.path() / "state.json";
    store.saveAtomic(path, sampleState());
    { std::ofstream output(path); output << R"({"schema_version":999})"; }
    EXPECT_EQ(store.load(path).status, StateLoadStatus::UnsupportedVersion);
}

TEST(ProjectionStateStoreTest, InvalidGeometryAndDuplicateIdentitiesAreRejected) {
    TempDirectory directory;
    ProjectionStateStore store;
    const auto path = directory.path() / "state.json";
    auto invalid = sampleState();
    invalid.windows[0].geometry.size.x = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(store.saveAtomic(path, invalid), std::invalid_argument);
    EXPECT_FALSE(std::filesystem::exists(path));

    invalid = sampleState();
    invalid.windows.push_back(invalid.windows.front());
    EXPECT_THROW(store.saveAtomic(path, invalid, true), std::invalid_argument);
    EXPECT_FALSE(std::filesystem::exists(path));
}

TEST(ProjectionStateStoreTest, ValidLastGoodBackupRecoversCorruptPrimary) {
    TempDirectory directory;
    ProjectionStateStore store;
    const auto path = directory.path() / "state.json";
    store.saveAtomic(path, sampleState());
    auto newer = sampleState();
    newer.windows[0].geometry.position.x = 1200.0;
    store.saveAtomic(path, newer);
    { std::ofstream output(path); output << "truncated"; }
    const auto loaded = store.load(path);
    EXPECT_EQ(loaded.status, StateLoadStatus::RecoveredBackup);
    ASSERT_TRUE(loaded.state);
    EXPECT_DOUBLE_EQ(loaded.state->windows[0].geometry.position.x, 0.0);
}

TEST(ProjectionStateStoreTest, AtomicReplacementLeavesCanonicalValidAndNoTemporaryFiles) {
    TempDirectory directory;
    ProjectionStateStore store;
    const auto path = directory.path() / "state.json";
    store.saveAtomic(path, sampleState());
    auto newer = sampleState();
    newer.windows[0].geometry.position.y = 900.0;
    store.saveAtomic(path, newer);
    const auto loaded = store.load(path);
    ASSERT_EQ(loaded.status, StateLoadStatus::Loaded);
    EXPECT_DOUBLE_EQ(loaded.state->windows[0].geometry.position.y, 900.0);
    EXPECT_TRUE(std::filesystem::exists(path.string() + ".bak"));
    for (const auto& entry : std::filesystem::directory_iterator(directory.path())) {
        EXPECT_EQ(entry.path().filename().string().find(".tmp."), std::string::npos);
    }
}

TEST(ProjectionStateStoreTest, CorruptCanonicalIsPreservedUnlessExplicitReplacementIsRequested) {
    TempDirectory directory;
    ProjectionStateStore store;
    const auto path = directory.path() / "state.json";
    { std::ofstream output(path); output << "broken"; }
    EXPECT_THROW(store.saveAtomic(path, sampleState()), std::runtime_error);
    { std::ifstream input(path); std::string contents; input >> contents; EXPECT_EQ(contents, "broken"); }
    store.saveAtomic(path, sampleState(), true);
    EXPECT_TRUE(std::filesystem::exists(path.string() + ".corrupt"));
    EXPECT_EQ(store.load(path).status, StateLoadStatus::Loaded);
}

TEST(ProjectionStateStoreTest, CorruptDiagnosticNamesAvoidExistingFiles) {
    TempDirectory directory;
    ProjectionStateStore store;
    const auto path = directory.path() / "state.json";
    { std::ofstream output(path); output << "first-bad"; }
    store.saveAtomic(path, sampleState(), true);
    { std::ofstream output(path); output << "second-bad"; }
    store.saveAtomic(path, sampleState(), true);
    std::ifstream first(path.string() + ".corrupt");
    std::ifstream second(path.string() + ".corrupt.1");
    std::string firstText, secondText;
    first >> firstText;
    second >> secondText;
    EXPECT_EQ(firstText, "first-bad");
    EXPECT_EQ(secondText, "second-bad");
    EXPECT_EQ(store.load(path).status, StateLoadStatus::Loaded);
}

TEST(SpatialRecoveryTest, SameLiveWindowRestoresPriorGeometryAndIdentity) {
    RuntimeMirror mirror;
    SpatialProjector original;
    World originalWorld;
    original.setCompositorEpoch("same-compositor");
    mirror.apply(WindowOpened{"abc", "foot", "Terminal", "1", 1});
    mirror.apply(WindowOpened{"def", "code", "Editor", "2", 2});
    original.reconcile(mirror, originalWorld);
    const auto savedGeometry = original.geometryForAddress("abc");
    const auto savedId = original.mappedId("abc");
    auto state = original.snapshot();
    original.endEpoch(originalWorld);

    SpatialProjector restored;
    World restoredWorld;
    restored.setCompositorEpoch("same-compositor");
    restored.restoreState(state, restoredWorld);
    RuntimeMirror current;
    current.apply(WindowTitleChanged{"0xABC", "Terminal renamed"});
    current.apply(WindowWorkspaceChanged{"abc", 1, "1"});
    restored.reconcile(current, restoredWorld);
    EXPECT_EQ(restored.geometryForAddress("abc")->position.x, savedGeometry->position.x);
    EXPECT_EQ(restored.geometryForAddress("abc")->position.y, savedGeometry->position.y);
    EXPECT_EQ(restored.mappedId("abc"), savedId);
    EXPECT_EQ(restored.findWindow("abc")->title, "Terminal renamed");
    EXPECT_EQ(restoredWorld.freeWindowCount(), 1U);
}

TEST(SpatialRecoveryTest, PersistedButStaleWindowDoesNotBecomeActive) {
    SpatialProjector restored;
    World world;
    restored.setCompositorEpoch("same-compositor");
    restored.restoreState(sampleState(), world);
    RuntimeMirror empty;
    restored.reconcile(empty, world);
    EXPECT_EQ(world.freeWindowCount(), 0U);
    EXPECT_TRUE(restored.windows().empty());
    EXPECT_EQ(restored.snapshot().windows.size(), 1U);
}

TEST(SpatialRecoveryTest, NewLiveWindowUsesPersistedRegionAndContinuedPlacementSlot) {
    SpatialProjector restored;
    World world;
    restored.setCompositorEpoch("different-compositor");
    auto state = sampleState();
    restored.restoreState(state, world);
    RuntimeMirror live;
    live.apply(WindowOpened{"new-address", "foot", "new terminal", "1", 1});
    restored.reconcile(live, world);
    const auto geometry = restored.geometryForAddress("new-address");
    ASSERT_TRUE(geometry);
    EXPECT_DOUBLE_EQ(geometry->position.x, 1200.0);
    EXPECT_DOUBLE_EQ(geometry->position.y, 0.0);
    EXPECT_EQ(restored.workspaceRegion("1")->origin.x, 0.0);
}

TEST(SpatialRecoveryTest, AddressReuseWithNewOpenEventDoesNotStealSavedGeometry) {
    SpatialProjector restored;
    World world;
    restored.setCompositorEpoch("same-compositor");
    restored.restoreState(sampleState(), world);
    RuntimeMirror live;
    live.apply(WindowOpened{"abc", "foot", "Terminal", "1", 1});
    restored.reconcile(live, world);
    EXPECT_NE(restored.mappedId("abc"), 1U);
    EXPECT_DOUBLE_EQ(restored.geometryForAddress("abc")->position.x, 1200.0);
}

TEST(SpatialRecoveryTest, AmbiguousSameClassAndTitleCandidatesReceiveFreshLayouts) {
    auto state = sampleState();
    state.windows.push_back({"def", "compositor-a", 2, "foot", "terminal", 1, "1", false,
        {{1200.0, 0.0}, {1000.0, 700.0}}, 1});
    state.nextWindowId = 3;
    state.workspaceSlots[0].nextSlot = 2;
    SpatialProjector restored;
    World world;
    restored.setCompositorEpoch("compositor-b");
    restored.restoreState(state, world);
    RuntimeMirror live;
    live.apply(WindowOpened{"111", "foot", "terminal", "1", 1});
    live.apply(WindowOpened{"222", "foot", "terminal", "1", 1});
    restored.reconcile(live, world);
    EXPECT_NE(restored.mappedId("111"), 1U);
    EXPECT_NE(restored.mappedId("222"), 2U);
    EXPECT_EQ(world.freeWindowCount(), 2U);
}

TEST(SpatialRecoveryTest, UniqueMetadataFingerprintCanRestoreReopenedApplication) {
    SpatialProjector restored;
    World world;
    restored.setCompositorEpoch("new-compositor");
    restored.restoreState(sampleState(), world);
    RuntimeMirror live;
    live.apply(WindowOpened{"new-address", "foot", "terminal", "1", 1});
    restored.reconcile(live, world);
    EXPECT_EQ(restored.mappedId("new-address"), 1U);
    EXPECT_DOUBLE_EQ(restored.geometryForAddress("new-address")->position.x, 0.0);
}

TEST(SpatialRecoveryTest, TitleChangeDoesNotInvalidateStrongSameEpochRestore) {
    auto state = sampleState();
    SpatialProjector restored;
    World world;
    restored.setCompositorEpoch("compositor-a");
    restored.restoreState(state, world);
    RuntimeMirror live;
    live.apply(WindowTitleChanged{"abc", "renamed browser/editor title"});
    live.apply(WindowWorkspaceChanged{"abc", 1, "1"});
    restored.reconcile(live, world);
    EXPECT_EQ(restored.mappedId("abc"), 1U);
    EXPECT_DOUBLE_EQ(restored.geometryForAddress("abc")->position.x, 0.0);
}

TEST(SpatialRecoveryTest, NamedAndSpecialWorkspaceRegionsAndCountersSurviveRestart) {
    RuntimeMirror mirror;
    SpatialProjector first;
    World firstWorld;
    first.setCompositorEpoch("epoch");
    mirror.apply(WindowOpened{"a", "one", "A", "dev", std::nullopt});
    mirror.apply(WindowOpened{"b", "two", "B", "special:chat", std::nullopt});
    first.reconcile(mirror, firstWorld);
    const auto dev = first.workspaceRegion("dev")->origin.x;
    const auto special = first.workspaceRegion("special:chat")->origin.x;
    const auto state = first.snapshot();
    first.endEpoch(firstWorld);

    SpatialProjector restored;
    World world;
    restored.setCompositorEpoch("epoch");
    restored.restoreState(state, world);
    RuntimeMirror live;
    live.apply(WindowOpened{"new", "app", "new", "numeric:99", std::nullopt});
    restored.reconcile(live, world);
    EXPECT_DOUBLE_EQ(restored.workspaceRegion("dev")->origin.x, dev);
    EXPECT_DOUBLE_EQ(restored.workspaceRegion("special:chat")->origin.x, special);
    EXPECT_DOUBLE_EQ(restored.workspaceRegion("numeric:99")->origin.x, 7200.0);
}

TEST(SpatialRecoveryTest, RestoreKeepsPersistedPlacementPolicyForFutureRegions) {
    InitialPlacementPolicy originalPolicy;
    originalPolicy.windowSize = {900.0, 600.0};
    originalPolicy.workspaceStride = 4000.0;
    originalPolicy.columns = 3;
    originalPolicy.gap = 100.0;
    originalPolicy.firstWorkspaceOrigin = {100.0, -50.0};
    SpatialProjector original(originalPolicy);
    original.setCompositorEpoch("epoch");
    RuntimeMirror existing;
    World originalWorld;
    existing.apply(WindowOpened{"old", "app", "old", "named", std::nullopt});
    original.reconcile(existing, originalWorld);
    const auto state = original.snapshot();
    original.endEpoch(originalWorld);

    InitialPlacementPolicy changedPolicy;
    changedPolicy.windowSize = {600.0, 400.0};
    changedPolicy.workspaceStride = 3000.0;
    SpatialProjector restored(changedPolicy);
    restored.setCompositorEpoch("another-epoch");
    World world;
    restored.restoreState(state, world);
    RuntimeMirror live;
    live.apply(WindowOpened{"new", "new-app", "new", "other", std::nullopt});
    restored.reconcile(live, world);
    const auto geometry = restored.geometryForAddress("new");
    ASSERT_TRUE(geometry);
    EXPECT_DOUBLE_EQ(geometry->size.x, 900.0);
    EXPECT_DOUBLE_EQ(geometry->size.y, 600.0);
    EXPECT_DOUBLE_EQ(restored.workspaceRegion("named")->origin.x, 100.0);
    EXPECT_DOUBLE_EQ(restored.workspaceRegion("other")->origin.x, 4100.0);
}

TEST(SpatialRecoveryTest, RestoreRejectsWindowPlacementSlotAtOrBeyondSavedCounter) {
    auto state = sampleState();
    state.windows[0].placementSlot = state.workspaceSlots[0].nextSlot;
    SpatialProjector projector;
    World world;
    EXPECT_THROW(projector.restoreState(state, world), std::invalid_argument);
}

TEST(SpatialRecoveryTest, EndEpochClearsWorldAndNextEpochDoesNotDuplicateWindows) {
    RuntimeBridge bridge;
    SpatialProjector projector;
    World world;
    projector.setCompositorEpoch("epoch");
    ASSERT_TRUE(bridge.processLine("openwindow>>abc,1,app,title"));
    projector.reconcile(bridge.mirror(), world);
    const auto id = projector.mappedId("abc");
    ASSERT_EQ(world.freeWindowCount(), 1U);
    bridge.endEpoch();
    EXPECT_FALSE(bridge.epochActive());
    projector.endEpoch(world);
    EXPECT_EQ(world.freeWindowCount(), 0U);
    EXPECT_FALSE(bridge.processLine("windowtitlev2>>abc,late"));
    bridge.beginEpoch();
    EXPECT_GT(bridge.connectionEpoch(), 1U);
    ASSERT_TRUE(bridge.processLine("windowtitlev2>>abc,renamed"));
    bridge.mirror().apply(WindowWorkspaceChanged{"abc", 1, "1"});
    projector.reconcile(bridge.mirror(), world);
    EXPECT_EQ(projector.mappedId("abc"), id);
    EXPECT_EQ(world.freeWindowCount(), 1U);
}

TEST(ProjectionStateManagerTest, DebounceFlushesOnlySpatialChangesAndCanBeDrivenByTestClock) {
    TempDirectory directory;
    const auto now = ProjectionStateManager::Clock::time_point{};
    ProjectionStateManager manager(directory.path() / "state.json", std::chrono::milliseconds{500});
    RuntimeMirror mirror;
    SpatialProjector projector;
    World world;
    projector.setCompositorEpoch("epoch");
    mirror.apply(WindowOpened{"abc", "app", "title", "1", 1});
    projector.reconcile(mirror, world);
    manager.observeLayout(projector, now);
    EXPECT_TRUE(manager.dirty());
    EXPECT_FALSE(manager.flushIfDue(now + std::chrono::milliseconds{499}, projector));
    mirror.apply(WindowTitleChanged{"abc", "title churn"});
    projector.reconcile(mirror, world);
    manager.observeLayout(projector, now + std::chrono::milliseconds{100});
    EXPECT_TRUE(manager.flushIfDue(now + std::chrono::milliseconds{500}, projector));
    EXPECT_FALSE(manager.dirty());
    EXPECT_EQ(manager.load().status, StateLoadStatus::Loaded);
}

TEST(ProjectionStateManagerTest, ExplicitSaveWorksWithoutDebounce) {
    TempDirectory directory;
    ProjectionStateManager manager(directory.path() / "state.json");
    SpatialProjector projector;
    projector.setCompositorEpoch("epoch");
    manager.saveNow(projector);
    EXPECT_EQ(manager.load().status, StateLoadStatus::Loaded);
}

TEST(ProjectionStateManagerTest, ContinuousChangesFlushAtMaximumDirtyAge) {
    TempDirectory directory;
    const auto start = ProjectionStateManager::Clock::time_point{};
    ProjectionStateManager manager(directory.path() / "state.json", std::chrono::milliseconds{500}, std::chrono::seconds{2});
    RuntimeMirror mirror;
    SpatialProjector projector;
    World world;
    projector.setCompositorEpoch("epoch");
    mirror.apply(WindowOpened{"one", "app", "one", "1", 1});
    projector.reconcile(mirror, world);
    manager.observeLayout(projector, start);
    for (int index = 1; index <= 5; ++index) {
        mirror.apply(WindowOpened{"window-" + std::to_string(index), "app", "title", "1", 1});
        projector.reconcile(mirror, world);
        manager.observeLayout(projector, start + std::chrono::milliseconds{index * 400});
    }
    EXPECT_FALSE(manager.flushIfDue(start + std::chrono::milliseconds{1999}, projector));
    EXPECT_TRUE(manager.flushIfDue(start + std::chrono::seconds{2}, projector));
    EXPECT_FALSE(manager.dirty());
}

TEST(ProjectionStateEndToEndTest, SaveDestroyRestoreReplayPreservesWorldSnapshot) {
    TempDirectory directory;
    ProjectionStateStore store;
    const auto path = directory.path() / "state.json";
    std::array<double, 4> expected{};
    {
        RuntimeBridge bridge;
        SpatialProjector projector;
        World world;
        projector.setCompositorEpoch("replay-compositor");
        replayLines(bridge, projector, world, {
            "windowtitlev2>>55f003fc2bd0,Home, Documents",
            "openwindow>>55f003fc2bd0,2,thunar,Home",
            "movewindowv2>>55f003fc2bd0,2,2",
            "activewindowv2>>55f003fc2bd0"});
        const auto geometry = projector.geometryForAddress("55f003fc2bd0");
        ASSERT_TRUE(geometry);
        expected = {geometry->position.x, geometry->position.y, geometry->size.x, geometry->size.y};
        store.saveAtomic(path, projector.snapshot());
    }
    {
        const auto loaded = store.load(path);
        ASSERT_EQ(loaded.status, StateLoadStatus::Loaded);
        RuntimeBridge bridge;
        SpatialProjector projector;
        World world;
        projector.setCompositorEpoch("replay-compositor");
        projector.restoreState(*loaded.state, world);
        replayLines(bridge, projector, world, {
            "windowtitlev2>>55f003fc2bd0,Renamed title",
            "movewindowv2>>55f003fc2bd0,2,2",
            "activewindowv2>>55f003fc2bd0"});
        ASSERT_EQ(world.freeWindowCount(), 1U);
        const auto geometry = projector.geometryForAddress("0x55F003FC2BD0");
        ASSERT_TRUE(geometry);
        EXPECT_DOUBLE_EQ(geometry->position.x, expected[0]);
        EXPECT_DOUBLE_EQ(geometry->position.y, expected[1]);
        EXPECT_DOUBLE_EQ(geometry->size.x, expected[2]);
        EXPECT_DOUBLE_EQ(geometry->size.y, expected[3]);
        EXPECT_EQ(projector.mappedId("55f003fc2bd0"), 1U);
    }
}
