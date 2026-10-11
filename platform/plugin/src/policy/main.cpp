// HyprVX Drift policy plugin `hyprvx-drift` (Feature 002). Nested-only: load it into a verified nested session with
// `scripts/nested.sh plugin load <path>`; never into the host compositor. Form P is loaded into the patched build
// (mechanism in the binary); form S into stock Hyprland, where it also installs the mechanism hooks (DV3-16).

#include "policy/Plugin.hpp"

#include <format>
#include <stdexcept>

#ifdef HYPRVX_FORM_S
namespace HyprVXSpike {
    void installMechanism(HANDLE handle);
}
#endif

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    HyprVXPolicy::PHANDLE = handle;

    if (const auto CHECK = HyprVXPolicy::checkLoad(handle); !CHECK.ok)
        throw std::runtime_error(std::format("[hyprvx] incompatible compositor: built for {}, running {}", CHECK.expected, CHECK.running));

#ifdef HYPRVX_FORM_S
    HyprVXSpike::installMechanism(handle);
#endif
    HyprVXPolicy::initSession(handle);

    return {"hyprvx-drift", "HyprVX Drift Mode policy (form " HYPRVX_FORM_NAME ")", "HyprVX", "0.2"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    HyprVXPolicy::shutdownSession();
}
