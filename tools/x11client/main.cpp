// hyprvx-x11client: deterministic X11 (XWayland) client for HyprVX native tests (contracts/test-tooling.md 1.4 4c).
//
//   hyprvx-x11client --size WxH --title T --log PATH --control FIFO [--menu X,Y,W,H] [--class C]
//
// Content as the Wayland test client (section 4): background RGB(32,32,32), 1 px grid every 50 px RGB(64,64,64),
// 16x16 corner markers TL red, TR green, BL blue, BR magenta. --menu prepares an override-redirect window with the popup
// palette (test-tooling 1.3), WM_TRANSIENT_FOR = the main window, mapped at main-window-relative X,Y by the control
// command "menu" and unmapped by "unmenu". Control: "redraw", "menu", "unmenu", "quit". Logs (JSON lines): enter,
// leave, motion, button and key events per window ("surface": "main"|"menu", X window-local coordinates), kb_enter on
// focus, and {"ev":"drawn","seq":N} after each redraw has been flushed and a round trip completed.
// Launch it only through `scripts/nested.sh run --x11`; it refuses any X server that is not the XWayland of an
// agent-prefix nested Hyprland (tools/common/NestedOnly.hpp).

#include <algorithm>
#include <cerrno>
#include <cinttypes>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <xcb/xcb.h>

#include "../common/NestedOnly.hpp"

namespace {

constexpr const char* kTool = "hyprvx-x11client";

struct Palette {
    uint32_t background;
    uint32_t grid;
    uint32_t markers[4]; // TL, TR, BL, BR
};
constexpr Palette kMainPalette = {0x202020, 0x404040, {0xFF0000, 0x00FF00, 0x0000FF, 0xFF00FF}};
constexpr Palette kPopupPalette = {0x282850, 0x404040, {0xFFFF00, 0x00FFFF, 0xFF8000, 0x8000FF}};
constexpr int kMarker = 16;
constexpr int kGridStep = 50;

struct Win {
    const char*    name = "";
    xcb_window_t   id = 0;
    int            w = 0, h = 0;
    const Palette* palette = nullptr;
    bool           mapped = false;
};

struct State {
    int               width = 800, height = 600;
    std::string       title = "hyprvx-x11client";
    std::string       wmClass = "hyprvx-x11client";
    std::string       logPath, controlPath;
    int               menuX = 0, menuY = 0, menuW = 0, menuH = 0;
    bool              hasMenu = false;
    FILE*             log = nullptr;
    xcb_connection_t* conn = nullptr;
    xcb_screen_t*     screen = nullptr;
    xcb_gcontext_t    gc = 0;
    Win               main{"main"}, menu{"menu"};
    uint64_t          seq = 0;
    bool              running = true;
};

uint64_t nowNs() {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000ull + static_cast<uint64_t>(ts.tv_nsec);
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

xcb_atom_t atom(State& s, const char* name) {
    auto* reply = xcb_intern_atom_reply(s.conn, xcb_intern_atom(s.conn, 0, std::strlen(name), name), nullptr);
    const xcb_atom_t a = reply ? reply->atom : static_cast<xcb_atom_t>(XCB_ATOM_NONE);
    std::free(reply);
    return a;
}

void sync(State& s) {
    std::free(xcb_get_input_focus_reply(s.conn, xcb_get_input_focus(s.conn), nullptr));
}

// The section 4 content at window resolution, uploaded as ZPixmap rows (32 bpp, depth 24).
void paint(State& s, Win& w) {
    std::vector<uint32_t> pixels(static_cast<size_t>(w.w) * w.h, w.palette->background);
    for (int y = 0; y < w.h; ++y)
        for (int x = 0; x < w.w; ++x)
            if (x % kGridStep == 0 || y % kGridStep == 0)
                pixels[static_cast<size_t>(y) * w.w + x] = w.palette->grid;
    const int corners[4][2] = {{0, 0}, {w.w - kMarker, 0}, {0, w.h - kMarker}, {w.w - kMarker, w.h - kMarker}};
    for (int m = 0; m < 4; ++m)
        for (int y = 0; y < kMarker; ++y)
            for (int x = 0; x < kMarker; ++x)
                pixels[static_cast<size_t>(corners[m][1] + y) * w.w + corners[m][0] + x] = w.palette->markers[m];
    // Rows per request below the maximum request length (in 4-byte units, minus the request header).
    const uint32_t maxBytes = xcb_get_maximum_request_length(s.conn) * 4 - 64;
    const int      rows = std::max(1, static_cast<int>(maxBytes / (static_cast<uint32_t>(w.w) * 4)));
    for (int y = 0; y < w.h; y += rows) {
        const int n = std::min(rows, w.h - y);
        xcb_put_image(s.conn, XCB_IMAGE_FORMAT_Z_PIXMAP, w.id, s.gc, w.w, n, 0, y, 0, s.screen->root_depth,
                      static_cast<uint32_t>(w.w) * n * 4, reinterpret_cast<const uint8_t*>(&pixels[static_cast<size_t>(y) * w.w]));
    }
}

void redraw(State& s) {
    paint(s, s.main);
    if (s.menu.mapped)
        paint(s, s.menu);
    xcb_flush(s.conn);
    sync(s);
    logLine(s, "\"ev\": \"drawn\", \"seq\": %" PRIu64, ++s.seq);
}

void createWindow(State& s, Win& w, bool overrideRedirect) {
    w.id = xcb_generate_id(s.conn);
    const uint32_t events = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_POINTER_MOTION |
        XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE |
        XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW | XCB_EVENT_MASK_FOCUS_CHANGE;
    const uint32_t values[] = {w.palette->background, overrideRedirect ? 1u : 0u, events};
    xcb_create_window(s.conn, XCB_COPY_FROM_PARENT, w.id, s.screen->root, 0, 0, w.w, w.h, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      s.screen->root_visual, XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK, values);
}

void mapMenu(State& s) {
    if (!s.hasMenu || s.menu.mapped)
        return;
    // Main-window-relative position in root coordinates.
    auto* t = xcb_translate_coordinates_reply(s.conn, xcb_translate_coordinates(s.conn, s.main.id, s.screen->root, s.menuX, s.menuY), nullptr);
    if (!t) {
        logLine(s, "\"ev\": \"error\", \"message\": \"cannot translate the menu position\"");
        return;
    }
    const int32_t  rootX = t->dst_x, rootY = t->dst_y;
    const uint32_t pos[] = {static_cast<uint32_t>(rootX), static_cast<uint32_t>(rootY)};
    std::free(t);
    xcb_configure_window(s.conn, s.menu.id, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, pos);
    xcb_map_window(s.conn, s.menu.id);
    s.menu.mapped = true;
    xcb_flush(s.conn);
    logLine(s, "\"ev\": \"menu_map\", \"root_x\": %d, \"root_y\": %d, \"w\": %d, \"h\": %d", rootX, rootY, s.menu.w, s.menu.h);
}

void handleCommand(State& s, const std::string& line) {
    if (line.empty())
        return;
    if (line == "redraw" || line == "sync") {
        redraw(s);
    } else if (line == "menu") {
        mapMenu(s);
        redraw(s);
    } else if (line == "unmenu") {
        if (s.menu.mapped) {
            xcb_unmap_window(s.conn, s.menu.id);
            s.menu.mapped = false;
        }
        redraw(s);
    } else if (line == "quit") {
        s.running = false;
    } else {
        logLine(s, "\"ev\": \"error\", \"message\": \"unknown control command\"");
    }
}

const char* nameOf(State& s, xcb_window_t id) {
    return id == s.main.id ? "main" : (id == s.menu.id ? "menu" : "other");
}

void handleEvent(State& s, xcb_generic_event_t* ev) {
    switch (ev->response_type & ~0x80) {
        case XCB_EXPOSE: {
            auto* e = reinterpret_cast<xcb_expose_event_t*>(ev);
            if (e->count == 0)
                redraw(s);
            break;
        }
        case XCB_CONFIGURE_NOTIFY: {
            auto* e = reinterpret_cast<xcb_configure_notify_event_t*>(ev);
            logLine(s, "\"ev\": \"configure\", \"surface\": \"%s\", \"x\": %d, \"y\": %d, \"w\": %d, \"h\": %d", nameOf(s, e->window), e->x, e->y,
                    e->width, e->height);
            break;
        }
        case XCB_MAP_NOTIFY:
            logLine(s, "\"ev\": \"map\", \"surface\": \"%s\"", nameOf(s, reinterpret_cast<xcb_map_notify_event_t*>(ev)->window));
            break;
        case XCB_ENTER_NOTIFY: {
            auto* e = reinterpret_cast<xcb_enter_notify_event_t*>(ev);
            logLine(s, "\"ev\": \"enter\", \"surface\": \"%s\", \"x\": %d.0, \"y\": %d.0", nameOf(s, e->event), e->event_x, e->event_y);
            break;
        }
        case XCB_LEAVE_NOTIFY:
            logLine(s, "\"ev\": \"leave\", \"surface\": \"%s\"", nameOf(s, reinterpret_cast<xcb_leave_notify_event_t*>(ev)->event));
            break;
        case XCB_MOTION_NOTIFY: {
            auto* e = reinterpret_cast<xcb_motion_notify_event_t*>(ev);
            logLine(s, "\"ev\": \"motion\", \"surface\": \"%s\", \"x\": %d.0, \"y\": %d.0", nameOf(s, e->event), e->event_x, e->event_y);
            break;
        }
        case XCB_BUTTON_PRESS:
        case XCB_BUTTON_RELEASE: {
            auto* e = reinterpret_cast<xcb_button_press_event_t*>(ev);
            // X buttons 1/2/3 are BTN_LEFT/MIDDLE/RIGHT (272/274/273); 4-7 are wheel steps.
            const int      d = e->detail;
            const unsigned button = d == 1 ? 272 : (d == 2 ? 274 : (d == 3 ? 273 : 0));
            const bool     pressed = (ev->response_type & ~0x80) == XCB_BUTTON_PRESS;
            if (button)
                logLine(s, "\"ev\": \"button\", \"surface\": \"%s\", \"button\": %u, \"state\": \"%s\", \"x\": %d.0, \"y\": %d.0", nameOf(s, e->event),
                        button, pressed ? "pressed" : "released", e->event_x, e->event_y);
            else if (pressed && d >= 4 && d <= 7)
                logLine(s, "\"ev\": \"axis\", \"axis\": \"%s\", \"value\": %d.0, \"surface\": \"%s\"", d <= 5 ? "v" : "h", (d == 4 || d == 6) ? -1 : 1,
                        nameOf(s, e->event));
            break;
        }
        case XCB_KEY_PRESS:
        case XCB_KEY_RELEASE: {
            auto* e = reinterpret_cast<xcb_key_press_event_t*>(ev);
            // XWayland keycodes are evdev codes + 8.
            logLine(s, "\"ev\": \"key\", \"key\": %d, \"state\": \"%s\", \"surface\": \"%s\"", e->detail - 8,
                    (ev->response_type & ~0x80) == XCB_KEY_PRESS ? "pressed" : "released", nameOf(s, e->event));
            break;
        }
        case XCB_FOCUS_IN:
            logLine(s, "\"ev\": \"kb_enter\", \"surface\": \"%s\"", nameOf(s, reinterpret_cast<xcb_focus_in_event_t*>(ev)->event));
            break;
        case XCB_FOCUS_OUT: logLine(s, "\"ev\": \"kb_leave\""); break;
        default: break;
    }
}

void parseArgs(State& s, int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (i + 1 >= argc)
            die(2, "missing value for " + a);
        const char* v = argv[++i];
        if (a == "--size") {
            if (std::sscanf(v, "%dx%d", &s.width, &s.height) != 2 || s.width < 2 * kMarker || s.height < 2 * kMarker || s.width > 8192 ||
                s.height > 8192)
                die(2, "--size must be WxH");
        } else if (a == "--title") {
            s.title = v;
        } else if (a == "--class") {
            s.wmClass = v;
        } else if (a == "--log") {
            s.logPath = v;
        } else if (a == "--control") {
            s.controlPath = v;
        } else if (a == "--menu") {
            if (std::sscanf(v, "%d,%d,%d,%d", &s.menuX, &s.menuY, &s.menuW, &s.menuH) != 4 || s.menuW < 2 * kMarker || s.menuH < 2 * kMarker)
                die(2, "--menu must be X,Y,W,H");
            s.hasMenu = true;
        } else {
            die(2, "unknown argument " + a);
        }
    }
    if (s.logPath.empty() || s.controlPath.empty())
        die(2, "usage: hyprvx-x11client --size WxH --title T --log PATH --control FIFO [--menu X,Y,W,H] [--class C]");
}

} // namespace

int main(int argc, char** argv) {
    State s;
    parseArgs(s, argc, argv);
    hyprvx::tools::checkNestedEnvironment(kTool, false);
    const char* display = std::getenv("DISPLAY");
    if (!display || display[0] != ':')
        die(3, "refusing: DISPLAY must be the nested XWayland display set by scripts/nested.sh run --x11");
    s.log = std::fopen(s.logPath.c_str(), "a");
    if (!s.log)
        die(1, "cannot open log " + s.logPath);

    int screenNumber = 0;
    s.conn = xcb_connect(nullptr, &screenNumber);
    if (!s.conn || xcb_connection_has_error(s.conn))
        die(1, "cannot connect to DISPLAY");
    hyprvx::tools::checkX11Peer(kTool, xcb_get_file_descriptor(s.conn));
    xcb_screen_iterator_t it = xcb_setup_roots_iterator(xcb_get_setup(s.conn));
    for (int i = 0; i < screenNumber; ++i)
        xcb_screen_next(&it);
    s.screen = it.data;
    if (!s.screen || s.screen->root_depth != 24)
        die(1, "unsupported X screen (depth 24 required)");

    s.main.w = s.width;
    s.main.h = s.height;
    s.main.palette = &kMainPalette;
    createWindow(s, s.main, false);
    const xcb_atom_t utf8 = atom(s, "UTF8_STRING");
    xcb_change_property(s.conn, XCB_PROP_MODE_REPLACE, s.main.id, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, s.title.size(), s.title.data());
    xcb_change_property(s.conn, XCB_PROP_MODE_REPLACE, s.main.id, atom(s, "_NET_WM_NAME"), utf8, 8, s.title.size(), s.title.data());
    // WM_CLASS is "instance\0class\0".
    const std::string wmClass = s.wmClass + '\0' + s.wmClass + '\0';
    xcb_change_property(s.conn, XCB_PROP_MODE_REPLACE, s.main.id, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8, wmClass.size(), wmClass.data());
    // Fixed size (PMinSize | PMaxSize), so the window keeps the logical size the tests expect.
    uint32_t hints[18] = {};
    hints[0] = (1u << 4) | (1u << 5);
    hints[5] = hints[7] = static_cast<uint32_t>(s.width);
    hints[6] = hints[8] = static_cast<uint32_t>(s.height);
    xcb_change_property(s.conn, XCB_PROP_MODE_REPLACE, s.main.id, XCB_ATOM_WM_NORMAL_HINTS, XCB_ATOM_WM_SIZE_HINTS, 32, 18, hints);

    s.gc = xcb_generate_id(s.conn);
    xcb_create_gc(s.conn, s.gc, s.main.id, 0, nullptr);
    if (s.hasMenu) {
        s.menu.w = s.menuW;
        s.menu.h = s.menuH;
        s.menu.palette = &kPopupPalette;
        createWindow(s, s.menu, true);
        xcb_change_property(s.conn, XCB_PROP_MODE_REPLACE, s.menu.id, XCB_ATOM_WM_TRANSIENT_FOR, XCB_ATOM_WINDOW, 32, 1, &s.main.id);
        const xcb_atom_t type = atom(s, "_NET_WM_WINDOW_TYPE");
        const xcb_atom_t popupMenu = atom(s, "_NET_WM_WINDOW_TYPE_POPUP_MENU");
        xcb_change_property(s.conn, XCB_PROP_MODE_REPLACE, s.menu.id, type, XCB_ATOM_ATOM, 32, 1, &popupMenu);
    }
    xcb_map_window(s.conn, s.main.id);
    s.main.mapped = true;
    xcb_flush(s.conn);
    logLine(s, "\"ev\": \"ready\", \"window\": %u", s.main.id);

    // O_RDWR keeps the FIFO open while writers come and go, so reads never see EOF.
    const int control = open(s.controlPath.c_str(), O_RDWR | O_CLOEXEC);
    if (control < 0)
        die(1, "cannot open control FIFO " + s.controlPath);
    std::string pending;
    while (s.running) {
        while (auto* ev = xcb_poll_for_event(s.conn)) {
            handleEvent(s, ev);
            std::free(ev);
        }
        if (xcb_connection_has_error(s.conn))
            die(1, "X connection lost");
        xcb_flush(s.conn);
        pollfd fds[2] = {{control, POLLIN, 0}, {xcb_get_file_descriptor(s.conn), POLLIN, 0}};
        if (poll(fds, 2, -1) < 0) {
            if (errno == EINTR)
                continue;
            die(1, "poll failed");
        }
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
    xcb_disconnect(s.conn);
    std::fclose(s.log);
    return 0;
}
