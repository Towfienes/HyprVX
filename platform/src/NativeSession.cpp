#include "hyprvx/platform/NativeSession.hpp"

#include "hyprvx/platform/OverlayState.hpp"
#include "hyprvx/platform/NativeEventBridge.hpp"
#include "hyprvx/hyprland/RuntimeBridge.hpp"
#include "hyprvx/hyprland/Socket2Stream.hpp"
#include "hyprvx/runtime/ProjectionStateStore.hpp"
#include "hyprvx/runtime/RuntimeCameraController.hpp"
#include "hyprvx/runtime/RuntimeNavigationInput.hpp"
#include "hyprvx/runtime/SpatialProjection.hpp"

// The upstream C scanner uses `namespace` as a parameter name in this
// protocol. Rename only that token while including the generated C header.
#define namespace namespace_
#include "wlr-layer-shell-client-protocol.h"
#undef namespace
#include "pointer-gestures-client-protocol.h"

#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include <linux/input-event-codes.h>
#include <linux/memfd.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <unordered_map>

namespace hyprvx::platform {
namespace {

using namespace hyprvx::runtime;
using Clock = std::chrono::steady_clock;

std::optional<KeyboardKey> keyFromSymbol(xkb_keysym_t symbol) {
    switch (xkb_keysym_to_lower(symbol)) {
    case XKB_KEY_h: return KeyboardKey::H;
    case XKB_KEY_j: return KeyboardKey::J;
    case XKB_KEY_k: return KeyboardKey::K;
    case XKB_KEY_l: return KeyboardKey::L;
    case XKB_KEY_Up: return KeyboardKey::Up;
    case XKB_KEY_Down: return KeyboardKey::Down;
    case XKB_KEY_Left: return KeyboardKey::Left;
    case XKB_KEY_Right: return KeyboardKey::Right;
    case XKB_KEY_Home: return KeyboardKey::Home;
    case XKB_KEY_r: return KeyboardKey::R;
    case XKB_KEY_1: return KeyboardKey::Digit1;
    case XKB_KEY_2: return KeyboardKey::Digit2;
    case XKB_KEY_3: return KeyboardKey::Digit3;
    case XKB_KEY_Escape: return KeyboardKey::Escape;
    default: return std::nullopt;
    }
}

bool validRect(const Rect& rect) noexcept {
    return std::isfinite(rect.left()) && std::isfinite(rect.top()) &&
        std::isfinite(rect.right()) && std::isfinite(rect.bottom()) &&
        rect.size.x > 0.0 && rect.size.y > 0.0;
}

bool intersects(const Rect& a, const Rect& b) noexcept {
    return validRect(a) && validRect(b) && a.left() < b.right() && a.right() > b.left() &&
        a.top() < b.bottom() && a.bottom() > b.top();
}

int connectSocket2(const NestedContract& contract) {
    const auto path = contract.runtimeDirectory / "hypr" / contract.signature / ".socket2.sock";
    const std::string name = path.string();
    if (name.size() >= sizeof(sockaddr_un::sun_path)) throw std::runtime_error("nested socket2 path too long");
    const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) throw std::runtime_error("could not create nested socket2 client");
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    std::copy(name.begin(), name.end(), address.sun_path);
    address.sun_path[name.size()] = '\0';
    if (::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        ::close(fd);
        throw std::runtime_error("could not connect guarded nested socket2");
    }
    struct ucred peer{};
    socklen_t size = sizeof(peer);
    if (::getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &peer, &size) != 0 || peer.pid != contract.compositorPid) {
        ::close(fd);
        throw std::runtime_error("nested socket2 peer changed during connection");
    }
    const int flags = ::fcntl(fd, F_GETFL);
    if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0) {
        ::close(fd);
        throw std::runtime_error("could not set socket2 nonblocking mode");
    }
    return fd;
}

struct ShmBuffer {
    wl_buffer* handle{};
    void* pixels{MAP_FAILED};
    std::size_t bytes{};
    int width{};
    int height{};
    bool busy{};
    ~ShmBuffer() {
        if (handle) wl_buffer_destroy(handle);
        if (pixels != MAP_FAILED) ::munmap(pixels, bytes);
    }
};

} // namespace

class NativeSession::Impl {
public:
    Impl(NestedContract contract, NativeSessionOptions options)
        : contract_(std::move(contract)), options_(std::move(options)),
          navigation_(camera_, world_, {1280.0, 720.0}, &projector_),
          dispatcher_(navigation_) {
        try {
        projector_.setCompositorEpoch(contract_.signature);
        if (!options_.statePath.empty()) {
        const auto loaded = ProjectionStateStore{}.load(options_.statePath);
            if (loaded.state) projector_.restoreState(*loaded.state, world_);
            else throw std::runtime_error("state restore refused: " + loaded.diagnostic);
        }
        overlay_.setInteractive(options_.interactive);
        display_ = wl_display_connect(contract_.waylandDisplay.c_str());
        if (!display_) throw std::runtime_error("could not connect nested Wayland display");
        socketFd_ = connectSocket2(contract_);
        eventPath_ = (contract_.runtimeDirectory / "hyprvx-native-events.sock").string();
        if (eventPath_.size() >= sizeof(sockaddr_un::sun_path))
            throw std::runtime_error("nested native event path too long");
        eventFd_ = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
        if (eventFd_ < 0) throw std::runtime_error("could not create native event listener");
        sockaddr_un nativeAddress{};
        nativeAddress.sun_family = AF_UNIX;
        std::copy(eventPath_.begin(), eventPath_.end(), nativeAddress.sun_path);
        nativeAddress.sun_path[eventPath_.size()] = '\0';
        if (::access(eventPath_.c_str(), F_OK) == 0) {
            if (::connect(eventFd_, reinterpret_cast<sockaddr*>(&nativeAddress), sizeof(nativeAddress)) == 0)
                throw std::runtime_error("another nested native event listener is active");
            if (errno != ECONNREFUSED) throw std::runtime_error("ambiguous native event listener path");
            ::unlink(eventPath_.c_str());
        }
        if (::bind(eventFd_, reinterpret_cast<sockaddr*>(&nativeAddress), sizeof(nativeAddress)) != 0 ||
            ::chmod(eventPath_.c_str(), 0600) != 0)
            throw std::runtime_error("could not bind private native event listener");
        eventBound_ = true;
        int passCredentials = 1;
        if (::setsockopt(eventFd_, SOL_SOCKET, SO_PASSCRED, &passCredentials, sizeof(passCredentials)) != 0)
            throw std::runtime_error("could not authenticate native event sender");
        sigset_t signals;
        sigemptyset(&signals);
        for (int value : {SIGTERM, SIGINT, SIGUSR1, SIGUSR2}) sigaddset(&signals, value);
        if (::sigprocmask(SIG_BLOCK, &signals, &previousSignalMask_) != 0)
            throw std::runtime_error("could not block native control signals");
        signalMaskInstalled_ = true;
        signalFd_ = ::signalfd(-1, &signals, SFD_CLOEXEC | SFD_NONBLOCK);
        if (signalFd_ < 0) throw std::runtime_error("could not create native signal fd");
        registry_ = wl_display_get_registry(display_);
        static const wl_registry_listener registryListener{
            .global = [](void* data, wl_registry* registry, uint32_t name, const char* interface, uint32_t version) {
                auto* self = static_cast<Impl*>(data);
                self->guardedCallback([&] { self->global(registry, name, interface, version); });
            },
            .global_remove = [](void* data, wl_registry*, uint32_t name) {
                auto* self = static_cast<Impl*>(data);
                self->guardedCallback([&] { self->globalRemoved(name); });
            }};
        wl_registry_add_listener(registry_, &registryListener, this);
        if (wl_display_roundtrip(display_) < 0) throw std::runtime_error("nested Wayland registry disconnected");
        if (!compositor_ || !shm_ || !layerShell_) throw std::runtime_error("nested compositor lacks required overlay globals");
        if (wl_display_roundtrip(display_) < 0) throw std::runtime_error("nested Wayland globals disconnected");
        if (!shmArgb_) throw std::runtime_error("nested compositor lacks ARGB8888 SHM format");
        surface_ = wl_compositor_create_surface(compositor_);
        if (!surface_) throw std::runtime_error("could not create overlay surface");
        static const wl_surface_listener surfaceListener{
            .enter = [](void* data, wl_surface*, wl_output* output) {
                auto* self = static_cast<Impl*>(data);
                self->guardedCallback([&] { self->surfaceOutput(output, true); });
            },
            .leave = [](void* data, wl_surface*, wl_output* output) {
                auto* self = static_cast<Impl*>(data);
                self->guardedCallback([&] { self->surfaceOutput(output, false); });
            }};
        wl_surface_add_listener(surface_, &surfaceListener, this);
        layer_ = zwlr_layer_shell_v1_get_layer_surface(layerShell_, surface_, nullptr,
            ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "hyprvx-native-dev");
        if (!layer_) throw std::runtime_error("nested compositor refused layer surface");
        static const zwlr_layer_surface_v1_listener layerListener{
            .configure = [](void* data, zwlr_layer_surface_v1* layer, uint32_t serial, uint32_t w, uint32_t h) {
                auto* self = static_cast<Impl*>(data);
                self->guardedCallback([&] { self->configured(layer, serial, w, h); });
            },
            .closed = [](void* data, zwlr_layer_surface_v1*) { static_cast<Impl*>(data)->running_ = false; }};
        zwlr_layer_surface_v1_add_listener(layer_, &layerListener, this);
        zwlr_layer_surface_v1_set_anchor(layer_,
            ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
            ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
        zwlr_layer_surface_v1_set_size(layer_, 0, 0);
        zwlr_layer_surface_v1_set_exclusive_zone(layer_, 0);
        applyInputMode();
        wl_surface_commit(surface_); // initial commit has no buffer
        if (wl_display_roundtrip(display_) < 0) throw std::runtime_error("nested layer-shell configure failed");
        if (options_.diagnostics) {
            std::cout.setf(std::ios::unitbuf);
            std::cout << "native overlay connected to nested " << contract_.signature << '\n';
        }
        } catch (...) {
            cleanup();
            throw;
        }
    }

    ~Impl() { cleanup(); }

    void cleanup() noexcept {
        dispatcher_.cancelInput();
        if (frame_) wl_callback_destroy(frame_);
        if (swipe_) zwp_pointer_gesture_swipe_v1_destroy(swipe_);
        if (pinch_) zwp_pointer_gesture_pinch_v1_destroy(pinch_);
        if (pointer_) wl_pointer_release(pointer_);
        if (keyboard_) wl_keyboard_release(keyboard_);
        if (xkbState_) xkb_state_unref(xkbState_);
        if (xkbKeymap_) xkb_keymap_unref(xkbKeymap_);
        if (xkbContext_) xkb_context_unref(xkbContext_);
        if (layer_) zwlr_layer_surface_v1_destroy(layer_);
        if (surface_) wl_surface_destroy(surface_);
        buffers_.clear();
        if (gestures_) zwp_pointer_gestures_v1_destroy(gestures_);
        if (seat_) wl_seat_release(seat_);
        if (output_) wl_output_release(output_);
        if (layerShell_) zwlr_layer_shell_v1_destroy(layerShell_);
        if (shm_) wl_shm_destroy(shm_);
        if (compositor_) wl_compositor_destroy(compositor_);
        if (registry_) wl_registry_destroy(registry_);
        if (display_) wl_display_disconnect(display_);
        if (socketFd_ >= 0) ::close(socketFd_);
        if (eventFd_ >= 0) ::close(eventFd_);
        if (eventBound_) ::unlink(eventPath_.c_str());
        if (signalFd_ >= 0) ::close(signalFd_);
        if (signalMaskInstalled_) ::sigprocmask(SIG_SETMASK, &previousSignalMask_, nullptr);
    }

    int run() {
        while (running_) {
            if (wl_display_dispatch_pending(display_) < 0) throw std::runtime_error("nested Wayland connection lost");
            if (dispatcher_.flushPendingPan()) overlay_.markDirty();
            drawIfNeeded();
            if (wl_display_flush(display_) < 0) throw std::runtime_error("nested Wayland flush failed");

            const bool active = navigation_.needsFrame() || mouse_.wheelActive();
            int timeout = active ? 8 : -1;
            if (mouse_.wheelActive()) {
                const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                    wheelDeadline_ - Clock::now()).count();
                timeout = std::min(timeout, static_cast<int>(std::max<std::int64_t>(0, remaining)));
            }
            pollfd fds[4]{{wl_display_get_fd(display_), POLLIN, 0}, {socketFd_, POLLIN, 0},
                          {signalFd_, POLLIN, 0}, {eventFd_, POLLIN, 0}};
            const int result = ::poll(fds, 4, timeout);
            if (result < 0) {
                if (errno == EINTR) continue;
                throw std::runtime_error("native event poll failed");
            }
            if (fds[0].revents & (POLLHUP | POLLERR | POLLNVAL)) throw std::runtime_error("nested Wayland display disconnected");
            if (fds[0].revents & POLLIN) {
                if (wl_display_dispatch(display_) < 0) throw std::runtime_error("nested Wayland dispatch failed");
            }
            if (fds[1].revents & (POLLIN | POLLHUP | POLLERR | POLLNVAL)) readSocket2();
            if (fds[3].revents & POLLIN) readNativeEvents();
            if (fds[2].revents & POLLIN) {
                signalfd_siginfo signal{};
                while (::read(signalFd_, &signal, sizeof(signal)) == sizeof(signal)) {
                    if (signal.ssi_signo == SIGTERM || signal.ssi_signo == SIGINT) running_ = false;
                    else if (signal.ssi_signo == SIGUSR1) setInteractive(!overlay_.interactive());
                    else if (signal.ssi_signo == SIGUSR2) {
                        resetInput();
                        navigation_.motion().stop();
                        camera_.setPosition({});
                        camera_.setZoom(1.0);
                        overlay_.markDirty();
                    }
                }
            }
            if (mouse_.wheelActive() && Clock::now() >= wheelDeadline_) {
                RuntimeNavigationAction action;
                if (mouse_.wheelEnd(action)) submit(action);
            }
            const auto now = Clock::now();
            const double dt = std::clamp(std::chrono::duration<double>(now - lastFrame_).count(), 0.0, 0.05);
            lastFrame_ = now;
            if (navigation_.needsFrame() && navigation_.tick(dt)) overlay_.markDirty();
        }
        if (callbackFailed_) {
            std::cerr << "native Wayland callback failed; stopped the overlay safely\n";
            return 2;
        }
        return 0;
    }

private:
    template <typename Callback>
    void guardedCallback(Callback&& callback) noexcept {
        try {
            callback();
        } catch (...) {
            // Exceptions must not cross the Wayland C callback boundary.
            callbackFailed_ = true;
            running_ = false;
        }
    }

    void global(wl_registry* registry, uint32_t name, const char* interface, uint32_t version);
    void globalRemoved(uint32_t name);
    void seatCapabilities(uint32_t capabilities);
    void installGestures();
    void surfaceOutput(wl_output* output, bool enter);
    void configured(zwlr_layer_surface_v1* layer, uint32_t serial, uint32_t width, uint32_t height);
    void applyInputMode();
    void setInteractive(bool interactive);
    void readSocket2();
    void readNativeEvents();
    void submit(const RuntimeNavigationAction& action);
    void drawIfNeeded();
    ShmBuffer* acquireBuffer();
    void rect(ShmBuffer& buffer, Rect worldRect, std::uint32_t color, bool filled);
    void pointerEnter(double x, double y) { pointerPosition_ = Vec2{x, y}; }
    void pointerLeave();
    void pointerMotion(double x, double y);
    void pointerButton(std::uint32_t button, bool pressed);
    void pointerAxis(std::uint32_t axis, double value);
    void keyboardMap(std::uint32_t format, int fd, std::uint32_t size);
    void keyboardKey(std::uint32_t key, bool pressed);
    void keyboardLeave();
    void resetInput();
    void gestureSwipeBegin(std::uint32_t fingers);
    void gestureSwipeUpdate(double dx, double dy);
    void gestureSwipeEnd(bool cancelled);
    void gesturePinchBegin(std::uint32_t fingers);
    void gesturePinchUpdate(double dx, double dy, double scale);
    void gesturePinchEnd(bool cancelled);

    NestedContract contract_;
    NativeSessionOptions options_;
    hyprvx::hyprland::RuntimeBridge bridge_;
    SpatialProjector projector_;
    World world_;
    Camera camera_;
    RuntimeCameraController navigation_;
    RuntimeNavigationActionDispatcher dispatcher_;
    KeyboardNavigationAdapter keyboardAdapter_;
    MouseNavigationAdapter mouse_;
    GestureNavigationAdapter gesture_;
    OverlayState overlay_;
    hyprvx::hyprland::LineFramer framer_;
    int socketFd_{-1};
    int signalFd_{-1};
    sigset_t previousSignalMask_{};
    bool signalMaskInstalled_{};
    int eventFd_{-1};
    std::string eventPath_;
    bool eventBound_{};
    std::unordered_map<std::string, ActualCompositorGeometry> actualGeometry_;
    bool running_{true};
    bool callbackFailed_{};
    bool shmArgb_{};
    Clock::time_point lastFrame_{Clock::now()};
    Clock::time_point wheelDeadline_{};
    std::optional<Vec2> pointerPosition_;
    bool swipePan_{};
    bool swipeFocus_{};
    double pinchPreviousScale_{1.0};

    wl_display* display_{};
    wl_registry* registry_{};
    wl_compositor* compositor_{};
    wl_shm* shm_{};
    wl_seat* seat_{};
    wl_output* output_{};
    wl_surface* surface_{};
    zwlr_layer_shell_v1* layerShell_{};
    zwlr_layer_surface_v1* layer_{};
    zwp_pointer_gestures_v1* gestures_{};
    zwp_pointer_gesture_swipe_v1* swipe_{};
    zwp_pointer_gesture_pinch_v1* pinch_{};
    wl_pointer* pointer_{};
    wl_keyboard* keyboard_{};
    wl_callback* frame_{};
    uint32_t compositorName_{};
    uint32_t shmName_{};
    uint32_t seatName_{};
    uint32_t outputName_{};
    uint32_t shellName_{};
    uint32_t gestureName_{};
    bool surfaceOnOutput_{};
    int outputScale_{1};
    xkb_context* xkbContext_{};
    xkb_keymap* xkbKeymap_{};
    xkb_state* xkbState_{};
    std::vector<std::unique_ptr<ShmBuffer>> buffers_;
};

NativeSession::NativeSession(NestedContract contract, NativeSessionOptions options)
    : impl_(std::make_unique<Impl>(std::move(contract), std::move(options))) {}
NativeSession::~NativeSession() = default;
int NativeSession::run() { return impl_->run(); }

void NativeSession::Impl::global(wl_registry* registry, uint32_t name, const char* interface, uint32_t version) {
    if (std::strcmp(interface, wl_compositor_interface.name) == 0 && !compositor_) {
        compositor_ = static_cast<wl_compositor*>(wl_registry_bind(registry, name, &wl_compositor_interface, std::min(version, 4U)));
        compositorName_ = name;
    } else if (std::strcmp(interface, wl_shm_interface.name) == 0 && !shm_) {
        shm_ = static_cast<wl_shm*>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
        shmName_ = name;
        static const wl_shm_listener listener{.format = [](void* data, wl_shm*, uint32_t format) {
            if (format == WL_SHM_FORMAT_ARGB8888) static_cast<Impl*>(data)->shmArgb_ = true;
        }};
        wl_shm_add_listener(shm_, &listener, this);
    } else if (std::strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0 && !layerShell_) {
        layerShell_ = static_cast<zwlr_layer_shell_v1*>(wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, std::min(version, 4U)));
        shellName_ = name;
    } else if (std::strcmp(interface, wl_seat_interface.name) == 0 && !seat_) {
        seat_ = static_cast<wl_seat*>(wl_registry_bind(registry, name, &wl_seat_interface, std::min(version, 5U)));
        seatName_ = name;
        static const wl_seat_listener listener{
            .capabilities = [](void* data, wl_seat*, uint32_t capabilities) {
                auto* self = static_cast<Impl*>(data);
                self->guardedCallback([&] { self->seatCapabilities(capabilities); });
            },
            .name = [](void*, wl_seat*, const char*) {}};
        wl_seat_add_listener(seat_, &listener, this);
    } else if (std::strcmp(interface, wl_output_interface.name) == 0 && !output_) {
        output_ = static_cast<wl_output*>(wl_registry_bind(registry, name, &wl_output_interface, std::min(version, 3U)));
        outputName_ = name;
        static const wl_output_listener listener{
            .geometry = [](void*, wl_output*, int32_t, int32_t, int32_t, int32_t, int32_t,
                           const char*, const char*, int32_t) {},
            .mode = [](void*, wl_output*, uint32_t, int32_t, int32_t, int32_t) {},
            .done = [](void*, wl_output*) {},
            .scale = [](void* data, wl_output*, int32_t scale) {
                auto* self = static_cast<Impl*>(data);
                self->guardedCallback([&] {
                    self->outputScale_ = std::clamp(scale, 1, 8);
                    if (self->surfaceOnOutput_ && self->overlay_.width() && self->overlay_.height())
                        self->overlay_.configure(self->overlay_.width(), self->overlay_.height(), self->outputScale_);
                });
            }};
        wl_output_add_listener(output_, &listener, this);
    } else if (std::strcmp(interface, zwp_pointer_gestures_v1_interface.name) == 0 && !gestures_) {
        gestures_ = static_cast<zwp_pointer_gestures_v1*>(wl_registry_bind(registry, name, &zwp_pointer_gestures_v1_interface, 1));
        gestureName_ = name;
        installGestures();
    }
}

void NativeSession::Impl::globalRemoved(uint32_t name) {
    if (name == seatName_) {
        seatCapabilities(0);
        wl_seat_release(seat_); seat_ = nullptr; seatName_ = 0;
    } else if (name == outputName_) {
        surfaceOnOutput_ = false;
        wl_output_release(output_); output_ = nullptr; outputName_ = 0;
        overlay_.markDirty();
    } else if (name == gestureName_) {
        if (swipe_) { zwp_pointer_gesture_swipe_v1_destroy(swipe_); swipe_ = nullptr; }
        if (pinch_) { zwp_pointer_gesture_pinch_v1_destroy(pinch_); pinch_ = nullptr; }
        zwp_pointer_gestures_v1_destroy(gestures_); gestures_ = nullptr; gestureName_ = 0;
        gesture_.reset(); dispatcher_.cancelInput();
    } else if (name == compositorName_ || name == shmName_ || name == shellName_) {
        running_ = false;
    }
}

void NativeSession::Impl::seatCapabilities(uint32_t capabilities) {
    if ((capabilities & WL_SEAT_CAPABILITY_POINTER) && !pointer_) {
        pointer_ = wl_seat_get_pointer(seat_);
        static const wl_pointer_listener listener{
            .enter = [](void* data, wl_pointer*, uint32_t, wl_surface*, wl_fixed_t x, wl_fixed_t y) {
                auto* self = static_cast<Impl*>(data);
                self->guardedCallback([&] { self->pointerEnter(wl_fixed_to_double(x), wl_fixed_to_double(y)); });
            },
            .leave = [](void* data, wl_pointer*, uint32_t, wl_surface*) {
                auto* self = static_cast<Impl*>(data);
                self->guardedCallback([&] { self->pointerLeave(); });
            },
            .motion = [](void* data, wl_pointer*, uint32_t, wl_fixed_t x, wl_fixed_t y) {
                auto* self = static_cast<Impl*>(data);
                self->guardedCallback([&] { self->pointerMotion(wl_fixed_to_double(x), wl_fixed_to_double(y)); });
            },
            .button = [](void* data, wl_pointer*, uint32_t, uint32_t, uint32_t button, uint32_t state) {
                auto* self = static_cast<Impl*>(data);
                self->guardedCallback([&] { self->pointerButton(button, state == WL_POINTER_BUTTON_STATE_PRESSED); });
            },
            .axis = [](void* data, wl_pointer*, uint32_t, uint32_t axis, wl_fixed_t value) {
                auto* self = static_cast<Impl*>(data);
                self->guardedCallback([&] { self->pointerAxis(axis, wl_fixed_to_double(value)); });
            },
            .frame = [](void*, wl_pointer*) {},
            .axis_source = [](void*, wl_pointer*, uint32_t) {},
            .axis_stop = [](void* data, wl_pointer*, uint32_t, uint32_t) {
                auto* self = static_cast<Impl*>(data);
                self->guardedCallback([&] {
                    RuntimeNavigationAction action;
                    if (self->mouse_.wheelEnd(action)) self->submit(action);
                });
            },
            .axis_discrete = [](void*, wl_pointer*, uint32_t, int32_t) {}};
        wl_pointer_add_listener(pointer_, &listener, this);
        installGestures();
    } else if (!(capabilities & WL_SEAT_CAPABILITY_POINTER) && pointer_) {
        if (swipe_) { zwp_pointer_gesture_swipe_v1_destroy(swipe_); swipe_ = nullptr; }
        if (pinch_) { zwp_pointer_gesture_pinch_v1_destroy(pinch_); pinch_ = nullptr; }
        wl_pointer_release(pointer_); pointer_ = nullptr;
        resetInput();
    }
    if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && !keyboard_) {
        keyboard_ = wl_seat_get_keyboard(seat_);
        static const wl_keyboard_listener listener{
            .keymap = [](void* data, wl_keyboard*, uint32_t format, int fd, uint32_t size) {
                auto* self = static_cast<Impl*>(data);
                self->guardedCallback([&] { self->keyboardMap(format, fd, size); });
            },
            .enter = [](void*, wl_keyboard*, uint32_t, wl_surface*, wl_array*) {},
            .leave = [](void* data, wl_keyboard*, uint32_t, wl_surface*) {
                auto* self = static_cast<Impl*>(data);
                self->guardedCallback([&] { self->keyboardLeave(); });
            },
            .key = [](void* data, wl_keyboard*, uint32_t, uint32_t, uint32_t key, uint32_t state) {
                auto* self = static_cast<Impl*>(data);
                self->guardedCallback([&] { self->keyboardKey(key, state == WL_KEYBOARD_KEY_STATE_PRESSED); });
            },
            .modifiers = [](void* data, wl_keyboard*, uint32_t, uint32_t depressed, uint32_t latched,
                            uint32_t locked, uint32_t group) {
                auto* self = static_cast<Impl*>(data);
                self->guardedCallback([&] {
                    if (self->xkbState_) xkb_state_update_mask(self->xkbState_, depressed, latched, locked, 0, 0, group);
                });
            },
            .repeat_info = [](void*, wl_keyboard*, int32_t, int32_t) {
                // Held-state pan is continuous; discrete semantic actions intentionally do not repeat.
            }};
        wl_keyboard_add_listener(keyboard_, &listener, this);
    } else if (!(capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && keyboard_) {
        wl_keyboard_release(keyboard_); keyboard_ = nullptr;
        keyboardLeave();
    }
}

void NativeSession::Impl::installGestures() {
    if (!gestures_ || !pointer_) return;
    if (!swipe_) {
        swipe_ = zwp_pointer_gestures_v1_get_swipe_gesture(gestures_, pointer_);
        static const zwp_pointer_gesture_swipe_v1_listener listener{
            .begin = [](void* data, zwp_pointer_gesture_swipe_v1*, uint32_t, uint32_t, wl_surface*, uint32_t fingers) {
                auto* self = static_cast<Impl*>(data);
                self->guardedCallback([&] { self->gestureSwipeBegin(fingers); });
            },
            .update = [](void* data, zwp_pointer_gesture_swipe_v1*, uint32_t, wl_fixed_t dx, wl_fixed_t dy) {
                auto* self = static_cast<Impl*>(data);
                self->guardedCallback([&] { self->gestureSwipeUpdate(wl_fixed_to_double(dx), wl_fixed_to_double(dy)); });
            },
            .end = [](void* data, zwp_pointer_gesture_swipe_v1*, uint32_t, uint32_t, int32_t cancelled) {
                auto* self = static_cast<Impl*>(data);
                self->guardedCallback([&] { self->gestureSwipeEnd(cancelled != 0); });
            }};
        zwp_pointer_gesture_swipe_v1_add_listener(swipe_, &listener, this);
    }
    if (!pinch_) {
        pinch_ = zwp_pointer_gestures_v1_get_pinch_gesture(gestures_, pointer_);
        static const zwp_pointer_gesture_pinch_v1_listener listener{
            .begin = [](void* data, zwp_pointer_gesture_pinch_v1*, uint32_t, uint32_t, wl_surface*, uint32_t fingers) {
                auto* self = static_cast<Impl*>(data);
                self->guardedCallback([&] { self->gesturePinchBegin(fingers); });
            },
            .update = [](void* data, zwp_pointer_gesture_pinch_v1*, uint32_t, wl_fixed_t dx,
                         wl_fixed_t dy, wl_fixed_t scale, wl_fixed_t) {
                auto* self = static_cast<Impl*>(data);
                self->guardedCallback([&] { self->gesturePinchUpdate(wl_fixed_to_double(dx), wl_fixed_to_double(dy), wl_fixed_to_double(scale)); });
            },
            .end = [](void* data, zwp_pointer_gesture_pinch_v1*, uint32_t, uint32_t, int32_t cancelled) {
                auto* self = static_cast<Impl*>(data);
                self->guardedCallback([&] { self->gesturePinchEnd(cancelled != 0); });
            }};
        zwp_pointer_gesture_pinch_v1_add_listener(pinch_, &listener, this);
    }
}

void NativeSession::Impl::surfaceOutput(wl_output* output, bool enter) {
    if (output != output_) return;
    surfaceOnOutput_ = enter;
    if (enter && overlay_.width() && overlay_.height())
        overlay_.configure(overlay_.width(), overlay_.height(), outputScale_);
    overlay_.markDirty();
}

void NativeSession::Impl::configured(zwlr_layer_surface_v1* layer, uint32_t serial,
                                     uint32_t width, uint32_t height) {
    zwlr_layer_surface_v1_ack_configure(layer, serial);
    if (width == 0) width = 1280;
    if (height == 0) height = 720;
    overlay_.configure(width, height, outputScale_);
    navigation_.setViewport({static_cast<double>(width), static_cast<double>(height)});
    wl_surface_set_buffer_scale(surface_, outputScale_);
    applyInputMode();
    if (options_.diagnostics) std::cout << "overlay configure " << width << 'x' << height
                                        << " scale=" << outputScale_ << '\n';
}

void NativeSession::Impl::applyInputMode() {
    if (!surface_ || !layer_ || !compositor_) return;
    auto* region = wl_compositor_create_region(compositor_);
    if (overlay_.interactive() && overlay_.width() && overlay_.height())
        wl_region_add(region, 0, 0, static_cast<int>(overlay_.width()), static_cast<int>(overlay_.height()));
    wl_surface_set_input_region(surface_, region);
    wl_region_destroy(region);
    zwlr_layer_surface_v1_set_keyboard_interactivity(layer_, overlay_.interactive()
        ? ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE
        : ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);
}

void NativeSession::Impl::setInteractive(bool interactive) {
    overlay_.setInteractive(interactive);
    applyInputMode();
    wl_surface_commit(surface_);
    if (!interactive) resetInput();
    if (options_.diagnostics) std::cout << "overlay " << (interactive ? "interactive" : "passive") << '\n';
}

void NativeSession::Impl::readSocket2() {
    std::array<char, 4096> bytes{};
    while (true) {
        const auto count = ::recv(socketFd_, bytes.data(), bytes.size(), 0);
        if (count < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) return;
            throw std::runtime_error("nested socket2 read failed");
        }
        if (count == 0) {
            bridge_.endEpoch();
            projector_.endEpoch(world_);
            navigation_.endEpoch();
            if (options_.diagnostics) std::cout << "nested socket2 disconnected; stopping overlay\n";
            running_ = false;
            return;
        }
        for (const auto& line : framer_.feed(std::string_view(bytes.data(), static_cast<std::size_t>(count)))) {
            const auto priorRevision = projector_.layoutRevision();
            const auto priorSelection = navigation_.selectedWindow();
            static_cast<void>(bridge_.processLine(line));
            projector_.reconcile(bridge_.mirror(), world_);
            navigation_.reconcileProjector();
            navigation_.observeCompositorSelection(projector_.focusedWindowId());
            if (projector_.layoutRevision() != priorRevision || navigation_.selectedWindow() != priorSelection)
                overlay_.markDirty();
            if (options_.diagnostics) std::cout << "socket2 " << line << '\n';
        }
    }
}

void NativeSession::Impl::readNativeEvents() {
    while (true) {
        std::array<char, 8192> bytes{};
        std::array<char, CMSG_SPACE(sizeof(ucred))> ancillary{};
        iovec payload{bytes.data(), bytes.size()};
        msghdr message{};
        message.msg_iov = &payload;
        message.msg_iovlen = 1;
        message.msg_control = ancillary.data();
        message.msg_controllen = ancillary.size();
        const auto count = ::recvmsg(eventFd_, &message, MSG_DONTWAIT);
        if (count < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) return;
            throw std::runtime_error("native event bridge read failed");
        }
        if (message.msg_flags & MSG_TRUNC) continue;
        bool authenticated = false;
        for (auto* header = CMSG_FIRSTHDR(&message); header; header = CMSG_NXTHDR(&message, header)) {
            if (header->cmsg_level == SOL_SOCKET && header->cmsg_type == SCM_CREDENTIALS &&
                header->cmsg_len >= CMSG_LEN(sizeof(ucred))) {
                const auto* credentials = reinterpret_cast<const ucred*>(CMSG_DATA(header));
                authenticated = credentials->pid == contract_.compositorPid && credentials->uid == ::getuid();
            }
        }
        if (!authenticated) continue;
        auto observation = parseNativeObservation(std::string_view(bytes.data(), static_cast<std::size_t>(count)));
        if (!observation) continue;
        if (observation->cameraReset) {
            resetInput();
            navigation_.motion().stop();
            camera_.setPosition({});
            camera_.setZoom(1.0);
            overlay_.markDirty();
        }
        if (observation->cameraPan) {
            submit({.type = RuntimeNavigationActionType::PanBegin});
            submit({.type = RuntimeNavigationActionType::PanDelta,
                .delta = {(*observation->cameraPan)[0], (*observation->cameraPan)[1]}});
            submit({.type = RuntimeNavigationActionType::PanEnd});
            if (dispatcher_.flushPendingPan()) overlay_.markDirty();
        }
        if (observation->actualGeometry)
            actualGeometry_[observation->actualGeometry->address] = *observation->actualGeometry;
        if (observation->semantic) {
            const auto priorRevision = projector_.layoutRevision();
            const auto priorSelection = navigation_.selectedWindow();
            bridge_.mirror().apply(*observation->semantic);
            projector_.reconcile(bridge_.mirror(), world_);
            navigation_.reconcileProjector();
            navigation_.observeCompositorSelection(projector_.focusedWindowId());
            if (projector_.layoutRevision() != priorRevision || navigation_.selectedWindow() != priorSelection)
                overlay_.markDirty();
            if (std::holds_alternative<hyprvx::hyprland::WindowClosed>(*observation->semantic))
                actualGeometry_.erase(std::get<hyprvx::hyprland::WindowClosed>(*observation->semantic).address);
        }
        if (options_.diagnostics) std::cout << "native event: " <<
            (observation->semantic ? hyprvx::hyprland::describe(*observation->semantic) : "control") << '\n';
    }
}

void NativeSession::Impl::submit(const RuntimeNavigationAction& action) {
    if (action.type == RuntimeNavigationActionType::Quit) {
        setInteractive(false); // Escape releases all nested input immediately.
        return;
    }
    if (dispatcher_.submit(action)) overlay_.markDirty();
}

void NativeSession::Impl::resetInput() {
    keyboardAdapter_.reset();
    mouse_.reset();
    gesture_.reset();
    dispatcher_.cancelInput();
    pointerPosition_.reset();
    swipePan_ = false;
    swipeFocus_ = false;
}

void NativeSession::Impl::pointerLeave() { resetInput(); }

void NativeSession::Impl::pointerMotion(double x, double y) {
    if (!overlay_.interactive()) return;
    const Vec2 current{x, y};
    if (pointerPosition_) {
        RuntimeNavigationAction action;
        if (mouse_.motion(current - *pointerPosition_, action)) submit(action);
    }
    pointerPosition_ = current;
}

void NativeSession::Impl::pointerButton(std::uint32_t button, bool pressed) {
    if (!overlay_.interactive()) return;
    RuntimeNavigationAction action;
    const auto translated = button == BTN_MIDDLE ? MouseButton::Middle :
        button == BTN_LEFT ? MouseButton::Left : MouseButton::Other;
    if (mouse_.button(translated, pressed, action)) submit(action);
    if (translated == MouseButton::Left && pressed && pointerPosition_) {
        const auto worldPoint = camera_.screenToWorld(*pointerPosition_);
        std::optional<WindowId> candidate;
        for (const auto& [id, window] : world_.freeWindows()) {
            const auto& geometry = window.geometry;
            if (worldPoint.x >= geometry.left() && worldPoint.x <= geometry.right() &&
                worldPoint.y >= geometry.top() && worldPoint.y <= geometry.bottom() &&
                (!candidate || id < *candidate)) candidate = id;
        }
        if (mouse_.click(candidate, action)) submit(action);
    }
}

void NativeSession::Impl::pointerAxis(std::uint32_t axis, double value) {
    if (!overlay_.interactive() || axis != WL_POINTER_AXIS_VERTICAL_SCROLL) return;
    RuntimeNavigationAction action;
    if (mouse_.wheelBegin(action)) submit(action);
    const Vec2 anchor = pointerPosition_.value_or(Vec2{overlay_.width() / 2.0, overlay_.height() / 2.0});
    if (mouse_.wheel(-value / 15.0, anchor, action)) submit(action);
    wheelDeadline_ = Clock::now() + std::chrono::milliseconds(180);
}

void NativeSession::Impl::keyboardMap(std::uint32_t format, int fd, std::uint32_t size) {
    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 || size == 0 || size > 16 * 1024 * 1024) { ::close(fd); return; }
    void* memory = ::mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    ::close(fd);
    if (memory == MAP_FAILED) throw std::runtime_error("could not map nested keyboard keymap");
    if (!xkbContext_) xkbContext_ = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    auto* next = xkb_keymap_new_from_string(xkbContext_, static_cast<const char*>(memory),
                                            XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
    ::munmap(memory, size);
    if (!next) throw std::runtime_error("nested keyboard keymap is invalid");
    if (xkbState_) xkb_state_unref(xkbState_);
    if (xkbKeymap_) xkb_keymap_unref(xkbKeymap_);
    xkbKeymap_ = next;
    xkbState_ = xkb_state_new(next);
    keyboardAdapter_.reset();
    dispatcher_.cancelInput();
}

void NativeSession::Impl::keyboardKey(std::uint32_t key, bool pressed) {
    if (!overlay_.interactive() || !xkbState_) return;
    const auto mapped = keyFromSymbol(xkb_state_key_get_one_sym(xkbState_, key + 8));
    if (!mapped) return;
    const KeyboardModifiers modifiers{
        xkb_state_mod_name_is_active(xkbState_, XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_EFFECTIVE) > 0,
        xkb_state_mod_name_is_active(xkbState_, XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE) > 0,
        xkb_state_mod_name_is_active(xkbState_, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE) > 0};
    RuntimeNavigationAction action;
    if (keyboardAdapter_.key(*mapped, pressed, modifiers, false, action)) submit(action);
}

void NativeSession::Impl::keyboardLeave() { resetInput(); }

void NativeSession::Impl::gestureSwipeBegin(std::uint32_t fingers) {
    if (!overlay_.interactive()) return;
    RuntimeNavigationAction action;
    swipePan_ = gesture_.panBegin(fingers, action);
    swipeFocus_ = false;
    if (swipePan_) submit(action);
    else swipeFocus_ = gesture_.swipeBegin(fingers);
}

void NativeSession::Impl::gestureSwipeUpdate(double dx, double dy) {
    if (!overlay_.interactive()) return;
    if (swipePan_) {
        RuntimeNavigationAction action;
        if (gesture_.panUpdate({dx / 1000.0, dy / 1000.0}, action)) submit(action);
    } else if (swipeFocus_) gesture_.swipeUpdate({dx / 1000.0, dy / 1000.0});
}

void NativeSession::Impl::gestureSwipeEnd(bool cancelled) {
    if (cancelled) { gesture_.reset(); dispatcher_.cancelInput(); }
    else {
        RuntimeNavigationAction action;
        if (swipePan_ && gesture_.panEnd(action)) submit(action);
        if (swipeFocus_ && gesture_.swipeEnd(action)) submit(action);
    }
    swipePan_ = swipeFocus_ = false;
}

void NativeSession::Impl::gesturePinchBegin(std::uint32_t fingers) {
    if (!overlay_.interactive()) return;
    RuntimeNavigationAction action;
    pinchPreviousScale_ = 1.0;
    if (gesture_.pinchBegin(fingers, action)) submit(action);
}

void NativeSession::Impl::gesturePinchUpdate(double dx, double dy, double scale) {
    if (!overlay_.interactive() || !std::isfinite(scale) || scale <= 0.0) return;
    if (pointerPosition_) { pointerPosition_->x += dx; pointerPosition_->y += dy; }
    const auto anchor = pointerPosition_.value_or(Vec2{overlay_.width() / 2.0, overlay_.height() / 2.0});
    RuntimeNavigationAction action;
    if (gesture_.pinchUpdate(scale / pinchPreviousScale_,
        {anchor.x / overlay_.width(), anchor.y / overlay_.height()},
        {static_cast<double>(overlay_.width()), static_cast<double>(overlay_.height())}, action)) {
        submit(action);
        pinchPreviousScale_ = scale;
    }
}

void NativeSession::Impl::gesturePinchEnd(bool cancelled) {
    if (cancelled) { gesture_.reset(); dispatcher_.cancelInput(); }
    else {
        RuntimeNavigationAction action;
        if (gesture_.pinchEnd(action)) submit(action);
    }
}

ShmBuffer* NativeSession::Impl::acquireBuffer() {
    const int width = static_cast<int>(overlay_.bufferWidth());
    const int height = static_cast<int>(overlay_.bufferHeight());
    if (width <= 0 || height <= 0 || static_cast<std::uint64_t>(width) * height > 16'000'000)
        throw std::runtime_error("overlay SHM buffer exceeds 64 MiB safety cap");
    buffers_.erase(std::remove_if(buffers_.begin(), buffers_.end(), [=](const auto& item) {
        return !item->busy && (item->width != width || item->height != height);
    }), buffers_.end());
    for (auto& item : buffers_) {
        if (!item->busy && item->width == width && item->height == height) return item.get();
    }
    if (buffers_.size() >= 3) return nullptr;
    auto item = std::make_unique<ShmBuffer>();
    item->width = width;
    item->height = height;
    item->bytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
    const int fd = static_cast<int>(::syscall(SYS_memfd_create, "hyprvx-overlay", MFD_CLOEXEC));
    if (fd < 0) throw std::runtime_error("memfd_create failed for overlay buffer");
    if (::ftruncate(fd, static_cast<off_t>(item->bytes)) != 0) {
        ::close(fd);
        throw std::runtime_error("ftruncate failed for overlay buffer");
    }
    item->pixels = ::mmap(nullptr, item->bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (item->pixels == MAP_FAILED) {
        ::close(fd);
        throw std::runtime_error("mmap failed for overlay buffer");
    }
    auto* pool = wl_shm_create_pool(shm_, fd, static_cast<int>(item->bytes));
    item->handle = wl_shm_pool_create_buffer(pool, 0, width, height, width * 4, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    ::close(fd);
    if (!item->handle) throw std::runtime_error("Wayland SHM buffer creation failed");
    static const wl_buffer_listener listener{.release = [](void* data, wl_buffer*) {
        static_cast<ShmBuffer*>(data)->busy = false;
    }};
    wl_buffer_add_listener(item->handle, &listener, item.get());
    auto* result = item.get();
    buffers_.push_back(std::move(item));
    return result;
}

void NativeSession::Impl::rect(ShmBuffer& buffer, Rect worldRect, std::uint32_t color, bool filled) {
    if (!validRect(worldRect)) return;
    const auto a = camera_.worldToScreen(worldRect.position);
    const auto b = camera_.worldToScreen({worldRect.right(), worldRect.bottom()});
    const double scale = static_cast<double>(overlay_.scale());
    const auto bounded = [](double value, int upper) -> int {
        if (!std::isfinite(value)) return value < 0 ? 0 : upper;
        return static_cast<int>(std::clamp(value, 0.0, static_cast<double>(upper)));
    };
    const int left = bounded(std::floor(a.x * scale), buffer.width);
    const int top = bounded(std::floor(a.y * scale), buffer.height);
    const int right = bounded(std::ceil(b.x * scale), buffer.width);
    const int bottom = bounded(std::ceil(b.y * scale), buffer.height);
    if (left >= right || top >= bottom) return;
    auto* pixels = static_cast<std::uint32_t*>(buffer.pixels);
    const int stroke = std::max(1, overlay_.scale() * 2);
    const auto fill = [&](int y, int begin, int end) {
        auto* row = pixels + static_cast<std::size_t>(y) * buffer.width;
        std::fill(row + begin, row + end, color);
    };
    for (int y = top; y < bottom; ++y) {
        if (filled || y < top + stroke || y >= bottom - stroke) fill(y, left, right);
        else {
            fill(y, left, std::min(left + stroke, right));
            if (right - stroke > left + stroke) fill(y, right - stroke, right);
            else fill(y, std::min(left + stroke, right), right);
        }
    }
}

void NativeSession::Impl::drawIfNeeded() {
    if (!overlay_.beginFrame()) return;
    auto* buffer = acquireBuffer();
    if (!buffer) { overlay_.frameFailed(); return; }
    std::memset(buffer->pixels, 0, buffer->bytes);
    const auto visible = camera_.visibleWorldRect({static_cast<double>(overlay_.width()),
                                                  static_cast<double>(overlay_.height())});
    for (const auto& [name, region] : projector_.workspaceMap().regions()) {
        static_cast<void>(name);
        if (intersects(region.bounds, visible)) rect(*buffer, region.bounds, 0x6672D0E8U, false);
    }
    for (const auto& [id, window] : world_.freeWindows()) {
        if (!intersects(window.geometry, visible)) continue;
        const bool selected = navigation_.selectedWindow() == id;
        rect(*buffer, window.geometry, selected ? 0xAAFFB347U : 0x774589D6U, false);
    }
    for (const auto& [id, island] : world_.islands()) {
        static_cast<void>(id);
        if (const auto bounds = island.worldBounds(); bounds && intersects(*bounds, visible))
            rect(*buffer, *bounds, 0x9958D68DU, false);
        for (const auto& member : island.members()) {
            if (const auto geometry = island.memberWorldGeometry(member.id); geometry && intersects(*geometry, visible))
                rect(*buffer, *geometry, 0x7758D68DU, false);
        }
    }
    const double statusSize = 12.0 / camera_.zoom();
    rect(*buffer, {camera_.screenToWorld({8.0, 8.0}), {statusSize, statusSize}},
         overlay_.interactive() ? 0xD0FFB347U : 0xD058D68DU, true);

    if (frame_) wl_callback_destroy(frame_);
    frame_ = wl_surface_frame(surface_);
    static const wl_callback_listener callbackListener{.done = [](void* data, wl_callback* callback, uint32_t) {
        auto* self = static_cast<Impl*>(data);
        if (self->frame_ == callback) self->frame_ = nullptr;
        wl_callback_destroy(callback);
        self->overlay_.frameDone();
    }};
    wl_callback_add_listener(frame_, &callbackListener, this);
    wl_surface_set_buffer_scale(surface_, overlay_.scale());
    buffer->busy = true;
    wl_surface_attach(surface_, buffer->handle, 0, 0);
    wl_surface_damage_buffer(surface_, 0, 0, buffer->width, buffer->height);
    wl_surface_commit(surface_);
}

} // namespace hyprvx::platform
