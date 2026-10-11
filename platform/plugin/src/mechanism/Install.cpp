// Form S mechanism installer (Feature 002; from the Feature 001 delivery spike's PLUGIN_INIT, T066): installs every
// mechanism hook or none, then the dev control in HYPRVX_DEV test builds. Called by the policy plugin's PLUGIN_INIT
// (src/policy/main.cpp) after the load-time compatibility check. Nested-only until a later delivery feature.

#include "Hooks.hpp"

#include <src/debug/log/Logger.hpp>

#include <format>

using namespace HyprVXSpike;

void HyprVXSpike::installMechanism(HANDLE handle) {
    PHANDLE = handle;

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
            throw std::runtime_error(std::format("[hyprvx] hook target not found (signature changed?): {}", spec.demangled));
        resolved.emplace_back(&spec, address);
    }

    for (auto const& [spec, address] : resolved) {
        auto* hook = HyprlandAPI::createFunctionHook(handle, address, spec->destination);
        if (!hook || !hook->hook())
            throw std::runtime_error(std::format("[hyprvx] hooking failed: {}", spec->demangled));
        if (spec->hook)
            *spec->hook = hook;
    }

#ifdef HYPRVX_DEV
    registerDevControl();
#endif
    Log::logger->log(Log::WARN, "[hyprvx] form S mechanism: {} function hooks installed", resolved.size());
}
