#pragma once

#include <string_view>

namespace hyprvx::platform {

inline constexpr std::string_view SupportedHyprlandCommit =
    "efb50993780079460b0cbed1363e2166a2de1d9f";

[[nodiscard]] constexpr bool pluginCommitCompatible(std::string_view compiled,
                                                     std::string_view running) noexcept {
    return compiled == SupportedHyprlandCommit && running == SupportedHyprlandCommit;
}

// What a running build's GIT_DIRTY string (read from `hyprctl version`) says about its source tree. Upstream Hyprland's
// CMake writes "clean" for an unmodified git tree and "dirty" otherwise, or takes $GIT_DIRTY / "unknown" without git
// (efb5099 CMakeLists.txt:197-244); distribution packages built from the git tag report "clean". HyprVX's own build
// tool writes "clean-upstream" for its stock prefix and "hyprvx-<patchset id>" for its patched prefix
// (tools/hyprvx_build.py).
enum class BuildTree { UnmodifiedUpstream, HyprvxPatched, Modified, Unknown };

[[nodiscard]] constexpr BuildTree classifyBuildTree(std::string_view dirty) noexcept {
    if (dirty == "clean" || dirty == "clean-upstream")
        return BuildTree::UnmodifiedUpstream;
    if (dirty.starts_with("hyprvx-"))
        return BuildTree::HyprvxPatched;
    if (dirty == "dirty")
        return BuildTree::Modified;
    return BuildTree::Unknown;
}

// Delivery form S (copied function bodies, DV3-16) runs only on an unmodified upstream build of the supported commit
// whose plugin ABI hash (commit plus aquamarine/hyprutils/hyprgraphics/hyprcursor/hyprlang major.minor,
// __hyprland_api_get_hash) equals the one the plugin was compiled against (__hyprland_api_get_client_hash).
[[nodiscard]] constexpr bool stockBuildCompatible(std::string_view runningCommit, std::string_view runningDirty,
                                                   std::string_view runningAbiHash,
                                                   std::string_view compiledAbiHash) noexcept {
    return runningCommit == SupportedHyprlandCommit &&
        classifyBuildTree(runningDirty) == BuildTree::UnmodifiedUpstream && !compiledAbiHash.empty() &&
        runningAbiHash == compiledAbiHash;
}

} // namespace hyprvx::platform
