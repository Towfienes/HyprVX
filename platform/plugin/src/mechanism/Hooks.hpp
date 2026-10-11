#pragma once

// Hook registry of the delivery spike. Every hook names its target by the exact demangled signature it was written
// against (stock Hyprland 0.56.2, efb5099); the plugin installs all hooks or none (main.cpp).

#include "SpikeAccess.hpp"

#include <src/plugins/PluginAPI.hpp>

#include <string>
#include <vector>

namespace HyprVXSpike {
    struct SHookSpec {
        std::string     name;      // substring of the mangled symbol (findFunctionsByName)
        std::string     demangled; // exact demangled signature that must match
        void*           destination;
        CFunctionHook** hook; // receives the installed hook (wrappers call its m_original); may be nullptr
    };

    inline HANDLE PHANDLE = nullptr;

    template <typename Fn>
    Fn original(CFunctionHook* hook) {
        return reinterpret_cast<Fn>(hook->m_original);
    }

    // Hand-written wrappers (RenderHooks.cpp) and generated copies (copied/*.cpp, gen_copies.py).
    void addRenderWrapperHooks(std::vector<SHookSpec>& specs);
    void addCopiedElementRendererHooks(std::vector<SHookSpec>& specs);
    void addCopiedOpenGLHooks(std::vector<SHookSpec>& specs);
    void addCopiedRendererHooks(std::vector<SHookSpec>& specs);
    void addCopiedWindowHooks(std::vector<SHookSpec>& specs);
    void addCopiedViewHitTesterHooks(std::vector<SHookSpec>& specs);
    void addCopiedInputManagerHooks(std::vector<SHookSpec>& specs);
    void addCopiedDragControllerHooks(std::vector<SHookSpec>& specs);
    void addCopiedPopupHooks(std::vector<SHookSpec>& specs);

    void registerDevControl();

    // Installs every hook (resolved by exact demangled signature first) or throws; then the dev control (HYPRVX_DEV).
    void installMechanism(HANDLE handle);
}
