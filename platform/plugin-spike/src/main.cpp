// HyprVX delivery spike plugin (T066). Nested-only: load it into a verified nested session of the stock-from-source
// prefix with `scripts/nested.sh run -- <prefix>/bin/hyprctl plugin load <path>`; never into the host instance.

#include "Hooks.hpp"

#include <src/debug/log/Logger.hpp>

#include <format>

using namespace HyprVXSpike;

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    if (const std::string HASH = __hyprland_api_get_hash(), CLIENT = __hyprland_api_get_client_hash(); HASH != CLIENT)
        throw std::runtime_error(std::format("[hyprvx-spike] built for Hyprland {}, running {}", CLIENT, HASH));

    std::vector<SHookSpec> specs;
    addRenderWrapperHooks(specs);
    addCopiedElementRendererHooks(specs);
    addCopiedOpenGLHooks(specs);
    addCopiedRendererHooks(specs);
    addCopiedWindowHooks(specs);
    addCopiedViewHitTesterHooks(specs);
    addCopiedInputManagerHooks(specs);
    addCopiedDragControllerHooks(specs);
    addCopiedPopupHooks(specs);

    // Resolve every target first: a missing or changed signature disables the plugin before anything is hooked.
    std::vector<std::pair<SHookSpec*, void*>> resolved;
    for (auto& spec : specs) {
        void* address = nullptr;
        for (auto const& match : HyprlandAPI::findFunctionsByName(handle, spec.name)) {
            if (match.demangled == spec.demangled) {
                address = match.address;
                break;
            }
        }
        if (!address)
            throw std::runtime_error(std::format("[hyprvx-spike] hook target not found (signature changed?): {}", spec.demangled));
        resolved.emplace_back(&spec, address);
    }

    for (auto const& [spec, address] : resolved) {
        auto* hook = HyprlandAPI::createFunctionHook(handle, address, spec->destination);
        if (!hook || !hook->hook())
            throw std::runtime_error(std::format("[hyprvx-spike] hooking failed: {}", spec->demangled));
        if (spec->hook)
            *spec->hook = hook;
    }

    registerDevControl();
    Log::logger->log(Log::WARN, "[hyprvx-spike] {} function hooks installed (nested-only delivery spike)", resolved.size());

    return {"hyprvx-plugin-spike", "HyprVX delivery spike: camera seams 0002-0009 as function hooks", "HyprVX", "0.1"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    ;
}
