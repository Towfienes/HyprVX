// hyprvx-vkeyboard: test-only virtual keyboard for the verified nested compositor (contracts/test-tooling.md 1.4 4b).
//
//   hyprvx-vkeyboard --log PATH --control FIFO
//
// Commands, one per line: "down KEY", "up KEY", "tap KEY", "quit", where KEY is a Linux evdev name (KEY_LEFTMETA,
// KEY_ESC, KEY_A, ...). A US keymap compiled by xkbcommon is sent once; modifier state follows every key change. Each
// command ends with a roundtrip, then a "done" log line, so the compositor has processed it before the line is
// written. Launch it only through scripts/nested.sh run; it refuses any display that is not a private /tmp/hvx-*
// runtime dir served by an agent-prefix Hyprland binary (tools/common/NestedOnly.hpp). Never uinput or ydotool.

#include <cerrno>
#include <cinttypes>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <utility>

#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <poll.h>
#include <sys/mman.h>
#include <unistd.h>

#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include "virtual-keyboard-unstable-v1-client-protocol.h"

#include "../common/NestedOnly.hpp"

namespace {

constexpr const char* kTool = "hyprvx-vkeyboard";

struct State {
    std::string                      logPath;
    std::string                      controlPath;
    FILE*                            log = nullptr;
    wl_display*                      display = nullptr;
    wl_seat*                         seat = nullptr;
    zwp_virtual_keyboard_manager_v1* manager = nullptr;
    zwp_virtual_keyboard_v1*         keyboard = nullptr;
    xkb_context*                     context = nullptr;
    xkb_keymap*                      keymap = nullptr;
    xkb_state*                       xkb = nullptr;
    uint64_t                         seq = 0;
    bool                             running = true;
};

// Evdev names accepted by the commands (the keys the native tests use, plus letters and digits).
constexpr std::pair<const char*, uint32_t> kKeys[] = {
    {"KEY_ESC", KEY_ESC},
    {"KEY_ENTER", KEY_ENTER},
    {"KEY_SPACE", KEY_SPACE},
    {"KEY_TAB", KEY_TAB},
    {"KEY_BACKSPACE", KEY_BACKSPACE},
    {"KEY_LEFTMETA", KEY_LEFTMETA},
    {"KEY_RIGHTMETA", KEY_RIGHTMETA},
    {"KEY_LEFTSHIFT", KEY_LEFTSHIFT},
    {"KEY_LEFTCTRL", KEY_LEFTCTRL},
    {"KEY_LEFTALT", KEY_LEFTALT},
    {"KEY_UP", KEY_UP},
    {"KEY_DOWN", KEY_DOWN},
    {"KEY_LEFT", KEY_LEFT},
    {"KEY_RIGHT", KEY_RIGHT},
    {"KEY_A", KEY_A}, {"KEY_B", KEY_B}, {"KEY_C", KEY_C}, {"KEY_D", KEY_D}, {"KEY_E", KEY_E}, {"KEY_F", KEY_F},
    {"KEY_G", KEY_G}, {"KEY_H", KEY_H}, {"KEY_I", KEY_I}, {"KEY_J", KEY_J}, {"KEY_K", KEY_K}, {"KEY_L", KEY_L},
    {"KEY_M", KEY_M}, {"KEY_N", KEY_N}, {"KEY_O", KEY_O}, {"KEY_P", KEY_P}, {"KEY_Q", KEY_Q}, {"KEY_R", KEY_R},
    {"KEY_S", KEY_S}, {"KEY_T", KEY_T}, {"KEY_U", KEY_U}, {"KEY_V", KEY_V}, {"KEY_W", KEY_W}, {"KEY_X", KEY_X},
    {"KEY_Y", KEY_Y}, {"KEY_Z", KEY_Z}, {"KEY_0", KEY_0}, {"KEY_1", KEY_1}, {"KEY_2", KEY_2}, {"KEY_3", KEY_3},
    {"KEY_4", KEY_4}, {"KEY_5", KEY_5}, {"KEY_6", KEY_6}, {"KEY_7", KEY_7}, {"KEY_8", KEY_8}, {"KEY_9", KEY_9},
};

uint64_t nowNs() {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000ull + static_cast<uint64_t>(ts.tv_nsec);
}

uint32_t nowMs() {
    return static_cast<uint32_t>(nowNs() / 1000000ull);
}

__attribute__((format(printf, 2, 3))) void logLine(State& s, const char* fmt, ...) {
    std::fprintf(s.log, "{\"t_ns\": %" PRIu64 ", ", nowNs());
    va_list args;
    va_start(args, fmt);
    std::vfprintf(s.log, fmt, args);
    va_end(args);
    std::fputs("}\n", s.log);
    std::fflush(s.log);
}

[[noreturn]] void die(int code, const std::string& message) {
    hyprvx::tools::refuse(kTool, code, message);
}

bool keyCode(const std::string& name, uint32_t& code) {
    for (const auto& [n, c] : kKeys) {
        if (name == n) {
            code = c;
            return true;
        }
    }
    return false;
}

// ---- registry ---------------------------------------------------------------------------------------------------

void registryGlobal(void* data, wl_registry* registry, uint32_t name, const char* interface, uint32_t) {
    auto& s = *static_cast<State*>(data);
    if (std::strcmp(interface, wl_seat_interface.name) == 0 && !s.seat)
        s.seat = static_cast<wl_seat*>(wl_registry_bind(registry, name, &wl_seat_interface, 1));
    else if (std::strcmp(interface, zwp_virtual_keyboard_manager_v1_interface.name) == 0)
        s.manager = static_cast<zwp_virtual_keyboard_manager_v1*>(
            wl_registry_bind(registry, name, &zwp_virtual_keyboard_manager_v1_interface, 1));
}
void registryRemove(void*, wl_registry*, uint32_t) {}
const wl_registry_listener kRegistryListener = {.global = registryGlobal, .global_remove = registryRemove};

// ---- keyboard ---------------------------------------------------------------------------------------------------

void sendKeymap(State& s) {
    s.context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    const xkb_rule_names names = {.rules = "evdev", .model = "pc105", .layout = "us", .variant = "", .options = ""};
    s.keymap = s.context ? xkb_keymap_new_from_names(s.context, &names, XKB_KEYMAP_COMPILE_NO_FLAGS) : nullptr;
    if (!s.keymap)
        die(1, "cannot compile the US keymap with xkbcommon");
    s.xkb = xkb_state_new(s.keymap);
    char* text = xkb_keymap_get_as_string(s.keymap, XKB_KEYMAP_FORMAT_TEXT_V1);
    if (!text || !s.xkb)
        die(1, "cannot serialize the keymap");
    const size_t size = std::strlen(text) + 1;
    const int    fd = memfd_create("hyprvx-vkeyboard-keymap", MFD_CLOEXEC);
    if (fd < 0 || ftruncate(fd, static_cast<off_t>(size)) != 0)
        die(1, "cannot create the keymap memfd");
    void* map = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED)
        die(1, "cannot map the keymap memfd");
    std::memcpy(map, text, size);
    munmap(map, size);
    std::free(text);
    zwp_virtual_keyboard_v1_keymap(s.keyboard, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd, static_cast<uint32_t>(size));
    close(fd);
}

void key(State& s, uint32_t code, bool pressed) {
    zwp_virtual_keyboard_v1_key(s.keyboard, nowMs(), code, pressed ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED);
    // XKB keycodes are evdev codes + 8.
    xkb_state_update_key(s.xkb, code + 8, pressed ? XKB_KEY_DOWN : XKB_KEY_UP);
    zwp_virtual_keyboard_v1_modifiers(s.keyboard, xkb_state_serialize_mods(s.xkb, XKB_STATE_MODS_DEPRESSED),
                                      xkb_state_serialize_mods(s.xkb, XKB_STATE_MODS_LATCHED),
                                      xkb_state_serialize_mods(s.xkb, XKB_STATE_MODS_LOCKED),
                                      xkb_state_serialize_layout(s.xkb, XKB_STATE_LAYOUT_EFFECTIVE));
    if (wl_display_roundtrip(s.display) < 0)
        die(1, "display lost");
}

void handleCommand(State& s, const std::string& line) {
    char     word[16] = {}, name[32] = {};
    uint32_t code = 0;
    if (line.empty())
        return;
    if (line == "quit") {
        s.running = false;
        return;
    }
    if (std::sscanf(line.c_str(), "%15s %31s", word, name) != 2 || !keyCode(name, code) ||
        (std::strcmp(word, "down") && std::strcmp(word, "up") && std::strcmp(word, "tap"))) {
        logLine(s, "\"ev\": \"error\", \"message\": \"unknown command or key\"");
        return;
    }
    if (!std::strcmp(word, "tap")) {
        key(s, code, true);
        key(s, code, false);
    } else {
        key(s, code, !std::strcmp(word, "down"));
    }
    logLine(s, "\"seq\": %" PRIu64 ", \"ev\": \"done\", \"cmd\": \"%s\", \"key\": \"%s\", \"code\": %u", ++s.seq, word, name, code);
}

} // namespace

int main(int argc, char** argv) {
    State s;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (i + 1 >= argc)
            die(2, "missing value for " + a);
        if (a == "--log")
            s.logPath = argv[++i];
        else if (a == "--control")
            s.controlPath = argv[++i];
        else
            die(2, "unknown argument " + a);
    }
    if (s.logPath.empty() || s.controlPath.empty())
        die(2, "usage: hyprvx-vkeyboard --log PATH --control FIFO");
    hyprvx::tools::checkNestedEnvironment(kTool);
    s.log = std::fopen(s.logPath.c_str(), "a");
    if (!s.log)
        die(1, "cannot open log " + s.logPath);

    s.display = wl_display_connect(nullptr);
    if (!s.display)
        die(1, "cannot connect to WAYLAND_DISPLAY");
    hyprvx::tools::checkWaylandPeer(kTool, wl_display_get_fd(s.display));
    wl_registry* registry = wl_display_get_registry(s.display);
    wl_registry_add_listener(registry, &kRegistryListener, &s);
    wl_display_roundtrip(s.display);
    if (!s.manager || !s.seat)
        die(1, "compositor lacks zwp_virtual_keyboard_manager_v1 or a seat");
    s.keyboard = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(s.manager, s.seat);
    sendKeymap(s);
    wl_display_roundtrip(s.display);
    logLine(s, "\"ev\": \"ready\"");

    // O_RDWR keeps the FIFO open while writers come and go, so reads never see EOF.
    const int control = open(s.controlPath.c_str(), O_RDWR | O_CLOEXEC);
    if (control < 0)
        die(1, "cannot open control FIFO " + s.controlPath);
    std::string pending;
    while (s.running) {
        pollfd fds[2] = {{control, POLLIN, 0}, {wl_display_get_fd(s.display), POLLIN, 0}};
        wl_display_flush(s.display);
        if (poll(fds, 2, -1) < 0) {
            if (errno == EINTR)
                continue;
            die(1, "poll failed");
        }
        if (fds[1].revents & (POLLERR | POLLHUP))
            die(1, "display lost");
        if ((fds[1].revents & POLLIN) && wl_display_dispatch(s.display) < 0)
            die(1, "display lost");
        if (fds[0].revents & POLLIN) {
            char          chunk[512];
            const ssize_t n = read(control, chunk, sizeof chunk);
            if (n > 0)
                pending.append(chunk, static_cast<size_t>(n));
            size_t newline;
            while (s.running && (newline = pending.find('\n')) != std::string::npos) {
                handleCommand(s, pending.substr(0, newline));
                pending.erase(0, newline + 1);
            }
        }
    }
    close(control);

    logLine(s, "\"ev\": \"quit\"");
    zwp_virtual_keyboard_v1_destroy(s.keyboard);
    wl_display_roundtrip(s.display);
    wl_display_disconnect(s.display);
    xkb_state_unref(s.xkb);
    xkb_keymap_unref(s.keymap);
    xkb_context_unref(s.context);
    std::fclose(s.log);
    return 0;
}
