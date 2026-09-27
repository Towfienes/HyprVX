#include "hyprvx/platform/NestedGuard.hpp"
#include "hyprvx/platform/PluginCompatibility.hpp"

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/desktop/Workspace.hpp>

#include <nlohmann/json.hpp>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr const char* kDispatcher = "hyprvx:control";

std::string addressOf(const PHLWINDOW& window) {
    std::ostringstream out;
    out << std::hex << reinterpret_cast<std::uintptr_t>(window.get());
    return out.str();
}

class PluginState {
public:
    explicit PluginState(HANDLE handle, hyprvx::platform::NestedContract contract)
        : handle_(handle), contract_(std::move(contract)) {
        const auto path = (contract_.runtimeDirectory / "hyprvx-native-events.sock").string();
        if (path.size() >= sizeof(sockaddr_un::sun_path))
            throw std::runtime_error("native bridge socket path too long");
        eventSocket_ = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
        if (eventSocket_ < 0) throw std::runtime_error("could not create native event socket");
        eventAddress_.sun_family = AF_UNIX;
        std::copy(path.begin(), path.end(), eventAddress_.sun_path);
        eventAddress_.sun_path[path.size()] = '\0';
    }

    ~PluginState() {
        enabled_ = false;
        listeners_.clear();
        if (dispatcherRegistered_) HyprlandAPI::removeDispatcher(handle_, kDispatcher);
        if (eventSocket_ >= 0) ::close(eventSocket_);
    }

    bool start() {
        dispatcherRegistered_ = HyprlandAPI::addDispatcherV2(handle_, kDispatcher,
            [this](std::string command) { return control(std::move(command)); });
        if (!dispatcherRegistered_) return false;
        auto& events = Event::bus()->m_events;
        listeners_.push_back(events.window.open.listen([this](PHLWINDOW window) { emitWindow("window_open", window); }));
        listeners_.push_back(events.window.close.listen([this](PHLWINDOW window) { emitWindow("window_close", window); }));
        listeners_.push_back(events.window.title.listen([this](PHLWINDOW window) { emitWindow("window_title", window); }));
        listeners_.push_back(events.window.floating.listen([this](PHLWINDOW window) { emitWindow("window_floating", window); }));
        listeners_.push_back(events.window.active.listen([this](PHLWINDOW window, Desktop::eFocusReason) {
            try { emit({{"kind", "focus"}, {"address", window ? addressOf(window) : ""}}); }
            catch (...) { /* A plugin callback must not unwind into Hyprland. */ }
        }));
        listeners_.push_back(events.window.moveToWorkspace.listen([this](PHLWINDOW window, PHLWORKSPACE workspace) {
            try {
                emit({{"kind", "window_move"}, {"address", window ? addressOf(window) : ""},
                      {"workspace", workspace ? workspace->m_name : ""}});
            } catch (...) { /* A plugin callback must not unwind into Hyprland. */ }
        }));
        listeners_.push_back(events.workspace.active.listen([this](PHLWORKSPACE workspace) {
            try { emit({{"kind", "workspace_active"}, {"workspace", workspace ? workspace->m_name : ""}}); }
            catch (...) { /* A plugin callback must not unwind into Hyprland. */ }
        }));
        return true;
    }

private:
    void emitWindow(const char* kind, const PHLWINDOW& window) noexcept {
        if (!window) return;
        try {
            nlohmann::json event{{"kind", kind}, {"address", addressOf(window)},
                {"workspace", window->m_workspace ? window->m_workspace->m_name : ""},
                {"class", window->m_class}, {"title", window->m_title},
                {"floating", window->m_isFloating}};
            if (const auto box = window->logicalBox(); box && std::isfinite(box->x) &&
                std::isfinite(box->y) && std::isfinite(box->w) && std::isfinite(box->h))
                event["actual_geometry"] = {box->x, box->y, box->w, box->h};
            emit(event);
        } catch (...) { /* Observation must never unwind into Hyprland. */ }
    }

    void emit(const nlohmann::json& event) noexcept {
        try {
            const auto payload = event.dump();
            if (payload.size() > 4096) return;
            ::sendto(eventSocket_, payload.data(), payload.size(), MSG_DONTWAIT | MSG_NOSIGNAL,
                reinterpret_cast<const sockaddr*>(&eventAddress_), sizeof(eventAddress_));
        } catch (...) { /* No subscriber is a valid state. */ }
    }

    SDispatchResult control(std::string command) noexcept {
        try {
            std::istringstream input(command);
            std::string verb;
            input >> verb;
            if (verb == "diagnostics")
                return {false, true, std::string("nested guarded; native content transform=") +
                    (enabled_ ? "diagnostic" : "disabled") + "; hit testing is untransformed"};
            if (verb == "reset" || verb == "disable") {
                enabled_ = false;
                emit({{"kind", "camera_reset"}});
                return {false, true, "identity compositor rendering restored"};
            }
            if (verb == "enable") {
                // A visual-only window transformer would move pixels while the compositor's
                // input hit test remains in the old logical coordinates. Refuse unsafe activation.
                enabled_ = false;
                return {false, false, "native content transform refused: no safe inverse hit-test hook in 0.56.2"};
            }
            if (verb == "pan") {
                double dx{}, dy{};
                if (!(input >> dx >> dy) || !std::isfinite(dx) || !std::isfinite(dy))
                    return {false, false, "usage: pan <finite-dx> <finite-dy>"};
                emit({{"kind", "camera_pan"}, {"dx", dx}, {"dy", dy}});
                return {false, true, "semantic pan sent to nested overlay"};
            }
            return {false, false, "commands: diagnostics, pan dx dy, enable, disable, reset"};
        } catch (...) { return {false, false, "plugin control failed safely"}; }
    }

    HANDLE handle_{};
    hyprvx::platform::NestedContract contract_;
    int eventSocket_{-1};
    sockaddr_un eventAddress_{};
    std::vector<CHyprSignalListener> listeners_;
    bool dispatcherRegistered_{};
    bool enabled_{};
};

std::unique_ptr<PluginState> state;

} // namespace

APICALL EXPORT std::string PLUGIN_API_VERSION() { return HYPRLAND_API_VERSION; }

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    try {
        auto contract = hyprvx::platform::requireNestedEnvironment(true);
        const auto version = HyprlandAPI::getHyprlandVersion(handle);
        if (!hyprvx::platform::pluginCommitCompatible(GIT_COMMIT_HASH, version.hash))
            return {"HyprVX refused", "unsupported Hyprland commit; plugin inactive", "HyprVX", "0.0"};
        auto next = std::make_unique<PluginState>(handle, std::move(contract));
        if (!next->start()) return {"HyprVX refused", "dispatcher registration failed; plugin inactive", "HyprVX", "0.0"};
        state = std::move(next);
        return {"HyprVX nested prototype", "guarded read-only event bridge and semantic control", "HyprVX", "0.1"};
    } catch (const std::exception& error) {
        return {"HyprVX refused", error.what(), "HyprVX", "0.0"};
    } catch (...) {
        return {"HyprVX refused", "unknown plugin initialization error", "HyprVX", "0.0"};
    }
}

APICALL EXPORT void PLUGIN_EXIT() { state.reset(); }
