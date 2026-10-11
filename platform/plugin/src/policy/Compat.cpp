// Build validation of the Drift policy plugin (research R10, FR-001). Reuses the V1 commit check
// (platform/include/hyprvx/platform/PluginCompatibility.hpp, Constitution VIII).

#include "policy/Plugin.hpp"

#include "../../../include/hyprvx/platform/PluginCompatibility.hpp"

#include <format>

namespace {
#ifdef HYPRVX_FORM_P
    constexpr std::string_view EXPECTED_DIRTY = "hyprvx-" HYPRVX_PATCHSET_ID;
#else
    constexpr std::string_view EXPECTED_DIRTY = "clean-upstream";
#endif

    std::string describe(std::string_view hash, std::string_view dirty) {
        return std::format("Hyprland {} ({})", hash.substr(0, 12), dirty);
    }

    // The running build's GIT_DIRTY string ("clean-upstream" or "hyprvx-<patchset id>"): SVersionInfo only reports a
    // bool, so it is read from `hyprctl version` ("... at commit <hash> <dirty> (<message>)."; SystemInfo.cpp).
    std::string runningDirty(const SVersionInfo& version) {
        const auto TEXT  = HyprlandAPI::invokeHyprctlCommand("version", "");
        const auto KEY   = "at commit " + version.hash + " ";
        const auto START = TEXT.find(KEY);
        if (START == std::string::npos)
            return "unknown";
        const auto FROM = START + KEY.size();
        const auto END  = TEXT.find(" (", FROM);
        return END == std::string::npos ? "unknown" : TEXT.substr(FROM, END - FROM);
    }
}

HyprVXPolicy::SBuildCheck HyprVXPolicy::checkLoad(HANDLE handle) {
#ifdef HYPRVX_FORM_S
    const std::string HASH = __hyprland_api_get_hash(), CLIENT = __hyprland_api_get_client_hash();
    if (HASH != CLIENT)
        return {false, "plugin API " + CLIENT, "plugin API " + HASH};
    const auto VERSION = HyprlandAPI::getHyprlandVersion(handle);
    if (const auto DIRTY = runningDirty(VERSION); DIRTY.starts_with("hyprvx-"))
        return {false, describe(hyprvx::platform::SupportedHyprlandCommit, EXPECTED_DIRTY), describe(VERSION.hash, DIRTY)};
#endif
    return {true, {}, {}};
}

// Research R10. Form S: the running commit is efb5099 and the build is clean upstream. Form P: the patchset id in
// GIT_DIRTY is the one this plugin was built against; the commit hash of a patched build is the commit of its patched
// tree (build-info source_commit), not efb5099, so it is not compared.
HyprVXPolicy::SBuildCheck HyprVXPolicy::checkEntering(HANDLE handle) {
    const auto VERSION = HyprlandAPI::getHyprlandVersion(handle);
    const auto DIRTY   = runningDirty(VERSION);
#ifdef HYPRVX_FORM_P
    const bool OK = DIRTY == EXPECTED_DIRTY;
    return {OK, std::format("patched Hyprland ({})", EXPECTED_DIRTY), describe(VERSION.hash, DIRTY)};
#else
    const bool OK = hyprvx::platform::pluginCommitCompatible(hyprvx::platform::SupportedHyprlandCommit, VERSION.hash) && DIRTY == EXPECTED_DIRTY;
    return {OK, describe(hyprvx::platform::SupportedHyprlandCommit, EXPECTED_DIRTY), describe(VERSION.hash, DIRTY)};
#endif
}

std::string HyprVXPolicy::buildJSON(HANDLE handle) {
    const auto VERSION = HyprlandAPI::getHyprlandVersion(handle);
    return std::format(R"({{"form":"{}","patchset":"{}","hyprland":"{}","running_dirty":"{}"}})", HYPRVX_FORM_NAME, HYPRVX_PATCHSET_ID, VERSION.hash, runningDirty(VERSION));
}
