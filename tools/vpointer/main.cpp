// hyprvx-vpointer: test-only virtual pointer for the verified nested compositor (contracts/test-tooling.md 4a).
//
//   hyprvx-vpointer --layout WxH --log PATH (--control FIFO | --script FILE)
//
// Commands, one per line: "move X Y" (absolute, logical px), "down [BUTTON]", "up [BUTTON]", "click [BUTTON]", "quit".
// Each command is followed by a frame request and a roundtrip, then a "done" log line, so the compositor has
// processed it (and sent any pointer events) before the line is written. Launch it only through scripts/nested.sh run;
// it refuses any display that is not a private /tmp/hvx-* runtime dir served by an agent-prefix Hyprland binary.

#include <algorithm>
#include <cerrno>
#include <cinttypes>
#include <climits>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <wayland-client.h>

#include "wlr-virtual-pointer-client-protocol.h"

namespace {

constexpr uint32_t kSubpixel = 256; // absolute motion resolution: 1/256 logical px
constexpr uint32_t kBtnLeft = 272;  // BTN_LEFT

struct Options {
    uint32_t    layoutW = 0;
    uint32_t    layoutH = 0;
    std::string logPath;
    std::string controlPath;
    std::string scriptPath;
};

struct State {
    Options                          opts;
    FILE*                            log = nullptr;
    wl_display*                      display = nullptr;
    wl_seat*                         seat = nullptr;
    zwlr_virtual_pointer_manager_v1* manager = nullptr;
    zwlr_virtual_pointer_v1*         pointer = nullptr;
    uint64_t                         seq = 0;
    bool                             running = true;
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
    std::fprintf(stderr, "hyprvx-vpointer: %s\n", message.c_str());
    std::exit(code);
}

// ---- nested-only guard ------------------------------------------------------------------------------------------

void checkEnvironment() {
    const char* runtime = std::getenv("XDG_RUNTIME_DIR");
    const char* display = std::getenv("WAYLAND_DISPLAY");
    if (!runtime || std::strncmp(runtime, "/tmp/hvx-", 9) != 0 || std::strchr(runtime + 5, '/'))
        die(3, "refusing: XDG_RUNTIME_DIR is not a private /tmp/hvx-* directory (use scripts/nested.sh run)");
    struct stat st{};
    if (lstat(runtime, &st) != 0 || !S_ISDIR(st.st_mode) || st.st_uid != getuid() || (st.st_mode & 0777) != 0700)
        die(3, "refusing: XDG_RUNTIME_DIR is not a directory owned by this user with mode 0700");
    if (!display || !*display || std::strchr(display, '/'))
        die(3, "refusing: WAYLAND_DISPLAY must be a bare socket name inside the nested runtime directory");
}

void checkPeer(wl_display* display) {
    ucred     cred{};
    socklen_t len = sizeof cred;
    if (getsockopt(wl_display_get_fd(display), SOL_SOCKET, SO_PEERCRED, &cred, &len) != 0)
        die(3, "refusing: cannot read the compositor PID (SO_PEERCRED)");
    char              exe[PATH_MAX] = {};
    const std::string link = "/proc/" + std::to_string(cred.pid) + "/exe";
    const ssize_t     n = readlink(link.c_str(), exe, sizeof exe - 1);
    const char*       home = std::getenv("HOME");
    if (n <= 0 || !home)
        die(3, "refusing: cannot resolve the compositor executable");
    const std::string path(exe, static_cast<size_t>(n));
    const std::string cacheRoot = std::string(home) + "/.cache/hyprvx/";
    if (path.rfind(cacheRoot, 0) != 0 || path.find("/prefix/") == std::string::npos)
        die(3, "refusing: the compositor is not an agent-prefix nested build (" + path + ")");
}

// ---- registry ---------------------------------------------------------------------------------------------------

void registryGlobal(void* data, wl_registry* registry, uint32_t name, const char* interface, uint32_t version) {
    auto& s = *static_cast<State*>(data);
    if (std::strcmp(interface, wl_seat_interface.name) == 0 && !s.seat)
        s.seat = static_cast<wl_seat*>(wl_registry_bind(registry, name, &wl_seat_interface, 1));
    else if (std::strcmp(interface, zwlr_virtual_pointer_manager_v1_interface.name) == 0)
        s.manager = static_cast<zwlr_virtual_pointer_manager_v1*>(
            wl_registry_bind(registry, name, &zwlr_virtual_pointer_manager_v1_interface, std::min<uint32_t>(version, 1)));
}
void registryRemove(void*, wl_registry*, uint32_t) {}
const wl_registry_listener kRegistryListener = {.global = registryGlobal, .global_remove = registryRemove};

// ---- commands ---------------------------------------------------------------------------------------------------

void finish(State& s) {
    zwlr_virtual_pointer_v1_frame(s.pointer);
    if (wl_display_roundtrip(s.display) < 0)
        die(1, "display lost");
}

void button(State& s, uint32_t code, bool pressed) {
    zwlr_virtual_pointer_v1_button(s.pointer, nowMs(), code,
                                   pressed ? WL_POINTER_BUTTON_STATE_PRESSED : WL_POINTER_BUTTON_STATE_RELEASED);
    finish(s);
}

void handleCommand(State& s, const std::string& line) {
    double   x = 0, y = 0;
    char     word[16] = {};
    unsigned code = kBtnLeft;
    if (line.empty())
        return;
    if (std::sscanf(line.c_str(), "move %lf %lf", &x, &y) == 2) {
        if (!std::isfinite(x) || !std::isfinite(y) || x < 0 || y < 0 || x > s.opts.layoutW || y > s.opts.layoutH) {
            logLine(s, "\"ev\": \"error\", \"message\": \"move outside the layout\"");
            return;
        }
        const uint32_t ex = s.opts.layoutW * kSubpixel, ey = s.opts.layoutH * kSubpixel;
        zwlr_virtual_pointer_v1_motion_absolute(s.pointer, nowMs(), static_cast<uint32_t>(std::lround(x * kSubpixel)),
                                                static_cast<uint32_t>(std::lround(y * kSubpixel)), ex, ey);
        finish(s);
        logLine(s, "\"seq\": %" PRIu64 ", \"ev\": \"done\", \"cmd\": \"move\", \"x\": %.4f, \"y\": %.4f", ++s.seq, x, y);
    } else if (std::sscanf(line.c_str(), "%15s %u", word, &code) >= 1 &&
               (!std::strcmp(word, "down") || !std::strcmp(word, "up") || !std::strcmp(word, "click"))) {
        if (!std::strcmp(word, "click")) {
            button(s, code, true);
            button(s, code, false);
        } else {
            button(s, code, !std::strcmp(word, "down"));
        }
        logLine(s, "\"seq\": %" PRIu64 ", \"ev\": \"done\", \"cmd\": \"%s\", \"button\": %u", ++s.seq, word, code);
    } else if (line == "quit") {
        s.running = false;
    } else {
        logLine(s, "\"ev\": \"error\", \"message\": \"unknown command\"");
    }
}

Options parseArgs(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto              next = [&]() -> const char* {
            if (i + 1 >= argc)
                die(2, "missing value for " + a);
            return argv[++i];
        };
        if (a == "--layout") {
            if (std::sscanf(next(), "%ux%u", &o.layoutW, &o.layoutH) != 2 || o.layoutW == 0 || o.layoutH == 0 ||
                o.layoutW > 65535 || o.layoutH > 65535)
                die(2, "--layout must be WxH");
        } else if (a == "--log") {
            o.logPath = next();
        } else if (a == "--control") {
            o.controlPath = next();
        } else if (a == "--script") {
            o.scriptPath = next();
        } else {
            die(2, "unknown argument " + a);
        }
    }
    if (o.layoutW == 0 || o.logPath.empty() || o.controlPath.empty() == o.scriptPath.empty())
        die(2, "usage: hyprvx-vpointer --layout WxH --log PATH (--control FIFO | --script FILE)");
    return o;
}

} // namespace

int main(int argc, char** argv) {
    State s;
    s.opts = parseArgs(argc, argv);
    checkEnvironment();
    s.log = std::fopen(s.opts.logPath.c_str(), "a");
    if (!s.log)
        die(1, "cannot open log " + s.opts.logPath);

    s.display = wl_display_connect(nullptr);
    if (!s.display)
        die(1, "cannot connect to WAYLAND_DISPLAY");
    checkPeer(s.display);
    wl_registry* registry = wl_display_get_registry(s.display);
    wl_registry_add_listener(registry, &kRegistryListener, &s);
    wl_display_roundtrip(s.display);
    if (!s.manager)
        die(1, "compositor lacks zwlr_virtual_pointer_manager_v1");
    s.pointer = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(s.manager, s.seat);
    wl_display_roundtrip(s.display);
    logLine(s, "\"ev\": \"ready\", \"layout\": [%u, %u]", s.opts.layoutW, s.opts.layoutH);

    if (!s.opts.scriptPath.empty()) {
        FILE* script = std::fopen(s.opts.scriptPath.c_str(), "r");
        if (!script)
            die(1, "cannot open script " + s.opts.scriptPath);
        char buf[256];
        while (s.running && std::fgets(buf, sizeof buf, script)) {
            std::string line(buf);
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
                line.pop_back();
            handleCommand(s, line);
        }
        std::fclose(script);
    } else {
        // O_RDWR keeps the FIFO open while writers come and go, so reads never see EOF.
        const int control = open(s.opts.controlPath.c_str(), O_RDWR | O_CLOEXEC);
        if (control < 0)
            die(1, "cannot open control FIFO " + s.opts.controlPath);
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
    }

    logLine(s, "\"ev\": \"quit\"");
    zwlr_virtual_pointer_v1_destroy(s.pointer);
    wl_display_roundtrip(s.display);
    wl_display_disconnect(s.display);
    std::fclose(s.log);
    return 0;
}
