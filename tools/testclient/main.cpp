// hyprvx-testclient: deterministic Wayland client for HyprVX native tests (contracts/test-tooling.md section 4).
//
//   hyprvx-testclient --size WxH --title T --log PATH [--control FIFO] [--layer TOP|OVERLAY] [--position X,Y]
//                     [--app-id ID] [--scale-aware] [--popup X,Y,W,H] [--subsurface X,Y,W,H] [--resizable]
//
// Content: background RGB(32,32,32), 1 px grid every 50 px RGB(64,64,64) from surface (0,0), 16x16 corner markers
// TL red, TR green, BL blue, BR magenta. SHM XRGB8888 (opaque). The control FIFO accepts
// "fill X Y W H RRGGBB", "redraw", "sync" and "quit". Every commit gets a frame callback; the log is JSON lines.
// --popup maps one xdg_popup and --subsurface one wl_subsurface at a fixed parent-local offset, each with its own
// palette (test-tooling 1.3); pointer events name the surface they are for.

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
#include <sys/mman.h>
#include <unistd.h>

#include <wayland-client.h>

// The generated layer-shell header names a parameter "namespace", a C++ keyword (same workaround as NativeSession).
#define namespace namespace_
#include "wlr-layer-shell-client-protocol.h"
#undef namespace
#include "xdg-shell-client-protocol.h"

namespace {

struct Palette {
    uint32_t background;
    uint32_t grid;
    uint32_t markers[4]; // TL, TR, BL, BR
};
constexpr Palette kMainPalette = {0x202020, 0x404040, {0xFF0000, 0x00FF00, 0x0000FF, 0xFF00FF}};
constexpr Palette kPopupPalette = {0x282850, 0x404040, {0xFFFF00, 0x00FFFF, 0xFF8000, 0x8000FF}};
constexpr Palette kSubPalette = {0x302820, 0x404040, {0xFF0080, 0x80FF00, 0x0080FF, 0x00FF80}};
constexpr uint32_t kBackground = kMainPalette.background;
constexpr int kMarker = 16;
constexpr int kGridStep = 50;

struct Rect {
    int  x = 0, y = 0, w = 0, h = 0;
    bool set = false;
};

struct Options {
    int width = 800;
    int height = 600;
    std::string title = "hyprvx-testclient";
    std::string appId = "hyprvx-testclient";
    std::string logPath;
    std::string controlPath;
    std::string layer; // empty: xdg_toplevel
    int posX = 0;
    int posY = 0;
    bool scaleAware = false;
    bool resizable = false; // no min/max size, so a compositor resize can change the window size
    Rect popup;
    Rect subsurface;
};

struct Buffer {
    wl_buffer* buffer = nullptr;
    uint32_t*  pixels = nullptr;
    bool       busy = false;
};

// A popup or subsurface: static content in one buffer, repainted only when the buffer scale changes.
struct Extra {
    const char*   name = "";
    Rect          rect;
    const Palette* palette = nullptr;
    wl_surface*   surface = nullptr;
    wl_buffer*    buffer = nullptr;
    uint32_t*     pixels = nullptr;
    size_t        size = 0;
};

struct Client {
    Options opts;
    FILE*   log = nullptr;

    wl_display*            display = nullptr;
    wl_compositor*         compositor = nullptr;
    uint32_t               compositorVersion = 0;
    wl_shm*                shm = nullptr;
    xdg_wm_base*           wmBase = nullptr;
    zwlr_layer_shell_v1*   layerShell = nullptr;
    wl_subcompositor*      subcompositor = nullptr;
    wl_seat*               seat = nullptr;
    wl_pointer*            pointer = nullptr;
    const char*            pointerSurface = "none"; // surface of the last pointer enter
    wl_surface*            surface = nullptr;
    xdg_surface*           xdgSurface = nullptr;
    xdg_toplevel*          toplevel = nullptr;
    zwlr_layer_surface_v1* layerSurface = nullptr;
    Extra                  popup;
    Extra                  sub;
    xdg_surface*           popupXdgSurface = nullptr;
    xdg_popup*             xdgPopup = nullptr;
    wl_subsurface*         subsurface = nullptr;

    int                   scale = 1;
    int                   bufferWidth = 0;
    int                   bufferHeight = 0;
    std::vector<uint32_t> canvas; // the content at buffer resolution
    Buffer                buffers[2];
    bool                  configured = false;
    bool                  running = true;
    uint64_t              seq = 0;
};

struct FrameRequest {
    Client*  client;
    uint64_t seq;
};

uint64_t nowNs() {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000ull + static_cast<uint64_t>(ts.tv_nsec);
}

__attribute__((format(printf, 2, 3))) void logLine(Client& c, const char* fmt, ...) {
    if (!c.log)
        return;
    std::fprintf(c.log, "{\"t_ns\": %" PRIu64 ", ", nowNs());
    va_list args;
    va_start(args, fmt);
    std::vfprintf(c.log, fmt, args);
    va_end(args);
    std::fputs("}\n", c.log);
    std::fflush(c.log);
}

[[noreturn]] void die(const std::string& message) {
    std::fprintf(stderr, "hyprvx-testclient: %s\n", message.c_str());
    std::exit(1);
}

// ---- content ----------------------------------------------------------------------------------------------------

void paintRect(Client& c, int x, int y, int w, int h, uint32_t color) {
    const int s = c.scale;
    const int x0 = std::max(0, x * s), y0 = std::max(0, y * s);
    const int x1 = std::min(c.bufferWidth, (x + w) * s), y1 = std::min(c.bufferHeight, (y + h) * s);
    for (int py = y0; py < y1; ++py)
        for (int px = x0; px < x1; ++px)
            c.canvas[static_cast<size_t>(py) * c.bufferWidth + px] = color;
}

// Background, 1 px grid every 50 px and the four 16x16 corner markers of a w x h logical surface at buffer scale s.
void paintContent(uint32_t* out, int w, int h, int s, const Palette& pal) {
    const int bw = w * s, bh = h * s;
    for (int py = 0; py < bh; ++py) {
        for (int px = 0; px < bw; ++px) {
            // A grid line is one logical pixel wide: the first buffer row/column of each 50 px step.
            const bool grid = ((px / s) % kGridStep == 0 && px % s == 0) || ((py / s) % kGridStep == 0 && py % s == 0);
            out[static_cast<size_t>(py) * bw + px] = grid ? pal.grid : pal.background;
        }
    }
    const int corners[4][2] = {{0, 0}, {w - kMarker, 0}, {0, h - kMarker}, {w - kMarker, h - kMarker}};
    for (int i = 0; i < 4; ++i)
        for (int py = corners[i][1] * s; py < (corners[i][1] + kMarker) * s; ++py)
            for (int px = corners[i][0] * s; px < (corners[i][0] + kMarker) * s; ++px)
                out[static_cast<size_t>(py) * bw + px] = pal.markers[i];
}

void paintBase(Client& c) {
    paintContent(c.canvas.data(), c.opts.width, c.opts.height, c.scale, kMainPalette);
}

// ---- buffers ----------------------------------------------------------------------------------------------------

void bufferRelease(void* data, wl_buffer*) {
    static_cast<Buffer*>(data)->busy = false;
}
const wl_buffer_listener kBufferListener = {.release = bufferRelease};

void createBuffers(Client& c) {
    const size_t oldSize = static_cast<size_t>(c.bufferWidth) * c.bufferHeight * 4;
    for (auto& b : c.buffers) {
        if (b.buffer) {
            wl_buffer_destroy(b.buffer);
            munmap(b.pixels, oldSize);
            b = {};
        }
    }
    c.bufferWidth = c.opts.width * c.scale;
    c.bufferHeight = c.opts.height * c.scale;
    c.canvas.assign(static_cast<size_t>(c.bufferWidth) * c.bufferHeight, kBackground);
    const size_t stride = static_cast<size_t>(c.bufferWidth) * 4;
    const size_t size = stride * c.bufferHeight;
    for (auto& b : c.buffers) {
        const int fd = memfd_create("hyprvx-testclient", MFD_CLOEXEC);
        if (fd < 0 || ftruncate(fd, static_cast<off_t>(size)) != 0)
            die("cannot allocate shm buffer");
        b.pixels = static_cast<uint32_t*>(mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
        if (b.pixels == MAP_FAILED)
            die("cannot map shm buffer");
        wl_shm_pool* pool = wl_shm_create_pool(c.shm, fd, static_cast<int32_t>(size));
        b.buffer = wl_shm_pool_create_buffer(pool, 0, c.bufferWidth, c.bufferHeight, static_cast<int32_t>(stride),
                                             WL_SHM_FORMAT_XRGB8888);
        wl_shm_pool_destroy(pool);
        close(fd);
        wl_buffer_add_listener(b.buffer, &kBufferListener, &b);
    }
}

// (Re)creates an extra surface's buffer at the client's buffer scale and attaches it; the caller commits.
void attachExtra(Client& c, Extra& e) {
    if (e.buffer) {
        wl_buffer_destroy(e.buffer);
        munmap(e.pixels, e.size);
    }
    const int bw = e.rect.w * c.scale, bh = e.rect.h * c.scale;
    const size_t stride = static_cast<size_t>(bw) * 4;
    e.size = stride * bh;
    const int fd = memfd_create("hyprvx-testclient-extra", MFD_CLOEXEC);
    if (fd < 0 || ftruncate(fd, static_cast<off_t>(e.size)) != 0)
        die("cannot allocate shm buffer");
    e.pixels = static_cast<uint32_t*>(mmap(nullptr, e.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
    if (e.pixels == MAP_FAILED)
        die("cannot map shm buffer");
    wl_shm_pool* pool = wl_shm_create_pool(c.shm, fd, static_cast<int32_t>(e.size));
    e.buffer = wl_shm_pool_create_buffer(pool, 0, bw, bh, static_cast<int32_t>(stride), WL_SHM_FORMAT_XRGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);
    paintContent(e.pixels, e.rect.w, e.rect.h, c.scale, *e.palette);
    wl_surface_set_buffer_scale(e.surface, c.scale);
    wl_surface_attach(e.surface, e.buffer, 0, 0);
    wl_surface_damage_buffer(e.surface, 0, 0, bw, bh);
}

void frameDone(void* data, wl_callback* callback, uint32_t) {
    auto* request = static_cast<FrameRequest*>(data);
    logLine(*request->client, "\"seq\": %" PRIu64 ", \"ev\": \"frame_done\"", request->seq);
    wl_callback_destroy(callback);
    delete request;
}
const wl_callback_listener kFrameListener = {.done = frameDone};

Buffer* freeBuffer(Client& c) {
    for (auto& b : c.buffers)
        if (!b.busy)
            return &b;
    return nullptr;
}

// Commits the canvas with damage {x, y, w, h} in surface coordinates, or a no-content commit for "sync".
void commit(Client& c, bool attach, int x, int y, int w, int h) {
    if (!c.configured)
        return;
    const uint64_t seq = ++c.seq;
    if (attach) {
        Buffer* target = freeBuffer(c);
        while (!target) {
            if (wl_display_dispatch(c.display) < 0)
                die("display lost while waiting for a free buffer");
            target = freeBuffer(c);
        }
        std::memcpy(target->pixels, c.canvas.data(), c.canvas.size() * 4);
        target->busy = true;
        wl_surface_attach(c.surface, target->buffer, 0, 0);
        wl_surface_damage_buffer(c.surface, x * c.scale, y * c.scale, w * c.scale, h * c.scale);
    }
    wl_callback* cb = wl_surface_frame(c.surface);
    wl_callback_add_listener(cb, &kFrameListener, new FrameRequest{&c, seq});
    wl_surface_commit(c.surface);
    if (attach)
        logLine(c, "\"seq\": %" PRIu64 ", \"ev\": \"commit\", \"damage\": [%d, %d, %d, %d]", seq, x, y, w, h);
    else
        logLine(c, "\"seq\": %" PRIu64 ", \"ev\": \"commit\", \"damage\": []", seq);
    wl_display_flush(c.display);
}

void redraw(Client& c) {
    paintBase(c);
    commit(c, true, 0, 0, c.opts.width, c.opts.height);
}

// ---- shell ------------------------------------------------------------------------------------------------------

void wmPing(void*, xdg_wm_base* base, uint32_t serial) {
    xdg_wm_base_pong(base, serial);
}
const xdg_wm_base_listener kWmBaseListener = {.ping = wmPing};

void popupXdgConfigure(void* data, xdg_surface* surface, uint32_t serial) {
    auto& c = *static_cast<Client*>(data);
    xdg_surface_ack_configure(surface, serial);
    if (!c.popup.buffer)
        attachExtra(c, c.popup);
    wl_surface_commit(c.popup.surface);
    wl_display_flush(c.display);
}
const xdg_surface_listener kPopupXdgSurfaceListener = {.configure = popupXdgConfigure};

void popupConfigure(void* data, xdg_popup*, int32_t x, int32_t y, int32_t w, int32_t h) {
    logLine(*static_cast<Client*>(data), "\"ev\": \"popup_configure\", \"x\": %d, \"y\": %d, \"w\": %d, \"h\": %d", x, y, w, h);
}
void popupDone(void* data, xdg_popup*) {
    logLine(*static_cast<Client*>(data), "\"ev\": \"popup_done\"");
}
void popupRepositioned(void*, xdg_popup*, uint32_t) {}
const xdg_popup_listener kPopupListener = {.configure = popupConfigure, .popup_done = popupDone, .repositioned = popupRepositioned};

void createPopup(Client& c) {
    const Rect& r = c.opts.popup;
    c.popup.surface = wl_compositor_create_surface(c.compositor);
    c.popupXdgSurface = xdg_wm_base_get_xdg_surface(c.wmBase, c.popup.surface);
    xdg_surface_add_listener(c.popupXdgSurface, &kPopupXdgSurfaceListener, &c);
    xdg_positioner* pos = xdg_wm_base_create_positioner(c.wmBase);
    xdg_positioner_set_size(pos, r.w, r.h);
    xdg_positioner_set_anchor_rect(pos, r.x, r.y, 1, 1);
    xdg_positioner_set_anchor(pos, XDG_POSITIONER_ANCHOR_TOP_LEFT);
    xdg_positioner_set_gravity(pos, XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT);
    xdg_positioner_set_constraint_adjustment(pos, XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_NONE);
    c.xdgPopup = xdg_surface_get_popup(c.popupXdgSurface, c.xdgSurface, pos);
    xdg_positioner_destroy(pos);
    xdg_popup_add_listener(c.xdgPopup, &kPopupListener, &c);
    wl_surface_commit(c.popup.surface);
}

// The subsurface position and stacking apply with the parent's next commit (the first redraw).
void createSubsurface(Client& c) {
    if (!c.subcompositor)
        die("compositor lacks wl_subcompositor");
    c.sub.surface = wl_compositor_create_surface(c.compositor);
    c.subsurface = wl_subcompositor_get_subsurface(c.subcompositor, c.sub.surface, c.surface);
    wl_subsurface_set_position(c.subsurface, c.opts.subsurface.x, c.opts.subsurface.y);
    wl_subsurface_place_above(c.subsurface, c.surface);
    wl_subsurface_set_desync(c.subsurface);
    attachExtra(c, c.sub);
    wl_surface_commit(c.sub.surface);
}

void xdgSurfaceConfigure(void* data, xdg_surface* surface, uint32_t serial) {
    auto& c = *static_cast<Client*>(data);
    xdg_surface_ack_configure(surface, serial);
    if (!c.configured) {
        c.configured = true;
        if (c.opts.subsurface.set)
            createSubsurface(c);
        redraw(c);
        if (c.opts.popup.set)
            createPopup(c);
    }
}
const xdg_surface_listener kXdgSurfaceListener = {.configure = xdgSurfaceConfigure};

// U0 spike: the configure states (xdg_wm_base v6: SUSPENDED = 9; ACTIVATED = 4) are logged.
void toplevelConfigure(void* data, xdg_toplevel*, int32_t w, int32_t h, wl_array* states) {
    std::string list;
    for (const auto* st = static_cast<const uint32_t*>(states->data); reinterpret_cast<const char*>(st) < static_cast<const char*>(states->data) + states->size; ++st)
        list += (list.empty() ? "" : ", ") + std::to_string(*st);
    logLine(*static_cast<Client*>(data), "\"ev\": \"configure\", \"w\": %d, \"h\": %d, \"states\": [%s]", w, h, list.c_str());
}
void toplevelClose(void* data, xdg_toplevel*) {
    static_cast<Client*>(data)->running = false;
}
void toplevelBounds(void*, xdg_toplevel*, int32_t, int32_t) {}
void toplevelCapabilities(void*, xdg_toplevel*, wl_array*) {}
const xdg_toplevel_listener kToplevelListener = {.configure = toplevelConfigure,
                                                 .close = toplevelClose,
                                                 .configure_bounds = toplevelBounds,
                                                 .wm_capabilities = toplevelCapabilities};

void layerConfigure(void* data, zwlr_layer_surface_v1* surface, uint32_t serial, uint32_t w, uint32_t h) {
    auto& c = *static_cast<Client*>(data);
    zwlr_layer_surface_v1_ack_configure(surface, serial);
    logLine(c, "\"ev\": \"configure\", \"w\": %u, \"h\": %u", w, h);
    if (!c.configured) {
        c.configured = true;
        redraw(c);
    }
}
void layerClosed(void* data, zwlr_layer_surface_v1*) {
    static_cast<Client*>(data)->running = false;
}
const zwlr_layer_surface_v1_listener kLayerListener = {.configure = layerConfigure, .closed = layerClosed};

// U0 spike: wl_surface.enter/leave of the main surface are logged.
void surfaceEnter(void* data, wl_surface*, wl_output*) {
    logLine(*static_cast<Client*>(data), "\"ev\": \"output_enter\"");
}
void surfaceLeave(void* data, wl_surface*, wl_output*) {
    logLine(*static_cast<Client*>(data), "\"ev\": \"output_leave\"");
}
void surfacePreferredScale(void* data, wl_surface*, int32_t factor) {
    auto& c = *static_cast<Client*>(data);
    logLine(c, "\"ev\": \"preferred_buffer_scale\", \"scale\": %d", factor);
    if (!c.opts.scaleAware || factor < 1 || factor == c.scale)
        return;
    c.scale = factor;
    createBuffers(c);
    wl_surface_set_buffer_scale(c.surface, c.scale);
    for (Extra* e : {&c.sub, &c.popup}) {
        if (e->buffer) {
            attachExtra(c, *e);
            wl_surface_commit(e->surface);
        }
    }
    redraw(c);
}
void surfacePreferredTransform(void*, wl_surface*, uint32_t) {}
const wl_surface_listener kSurfaceListener = {.enter = surfaceEnter,
                                              .leave = surfaceLeave,
                                              .preferred_buffer_scale = surfacePreferredScale,
                                              .preferred_buffer_transform = surfacePreferredTransform};

// ---- pointer log ------------------------------------------------------------------------------------------------

const char* surfaceName(const Client& c, const wl_surface* s) {
    if (s && s == c.surface)
        return "main";
    if (s && s == c.popup.surface)
        return "popup";
    if (s && s == c.sub.surface)
        return "sub";
    return "unknown";
}

void pointerEnter(void* data, wl_pointer*, uint32_t, wl_surface* surface, wl_fixed_t x, wl_fixed_t y) {
    auto& c = *static_cast<Client*>(data);
    c.pointerSurface = surfaceName(c, surface);
    logLine(c, "\"ev\": \"enter\", \"surface\": \"%s\", \"x\": %.4f, \"y\": %.4f", c.pointerSurface, wl_fixed_to_double(x),
            wl_fixed_to_double(y));
}
void pointerLeave(void* data, wl_pointer*, uint32_t, wl_surface* surface) {
    auto& c = *static_cast<Client*>(data);
    logLine(c, "\"ev\": \"leave\", \"surface\": \"%s\"", surfaceName(c, surface));
    c.pointerSurface = "none";
}
void pointerMotion(void* data, wl_pointer*, uint32_t, wl_fixed_t x, wl_fixed_t y) {
    auto& c = *static_cast<Client*>(data);
    logLine(c, "\"ev\": \"motion\", \"surface\": \"%s\", \"x\": %.4f, \"y\": %.4f", c.pointerSurface, wl_fixed_to_double(x),
            wl_fixed_to_double(y));
}
void pointerButton(void* data, wl_pointer*, uint32_t, uint32_t, uint32_t button, uint32_t state) {
    auto& c = *static_cast<Client*>(data);
    logLine(c, "\"ev\": \"button\", \"surface\": \"%s\", \"button\": %u, \"state\": \"%s\"", c.pointerSurface, button,
            state == WL_POINTER_BUTTON_STATE_PRESSED ? "pressed" : "released");
}
void pointerAxis(void*, wl_pointer*, uint32_t, uint32_t, wl_fixed_t) {}
void pointerFrame(void*, wl_pointer*) {}
void pointerAxisSource(void*, wl_pointer*, uint32_t) {}
void pointerAxisStop(void*, wl_pointer*, uint32_t, uint32_t) {}
void pointerAxisDiscrete(void*, wl_pointer*, uint32_t, int32_t) {}
void pointerAxisValue120(void*, wl_pointer*, uint32_t, int32_t) {}
void pointerAxisRelative(void*, wl_pointer*, uint32_t, uint32_t) {}
void pointerWarp(void* data, wl_pointer*, wl_fixed_t x, wl_fixed_t y) {
    logLine(*static_cast<Client*>(data), "\"ev\": \"warp\", \"x\": %.4f, \"y\": %.4f", wl_fixed_to_double(x), wl_fixed_to_double(y));
}
const wl_pointer_listener kPointerListener = {.enter = pointerEnter,
                                              .leave = pointerLeave,
                                              .motion = pointerMotion,
                                              .button = pointerButton,
                                              .axis = pointerAxis,
                                              .frame = pointerFrame,
                                              .axis_source = pointerAxisSource,
                                              .axis_stop = pointerAxisStop,
                                              .axis_discrete = pointerAxisDiscrete,
                                              .axis_value120 = pointerAxisValue120,
                                              .axis_relative_direction = pointerAxisRelative,
                                              .warp = pointerWarp};

void seatCapabilities(void* data, wl_seat* seat, uint32_t caps) {
    auto& c = *static_cast<Client*>(data);
    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !c.pointer) {
        c.pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(c.pointer, &kPointerListener, &c);
    }
}
void seatName(void*, wl_seat*, const char*) {}
const wl_seat_listener kSeatListener = {.capabilities = seatCapabilities, .name = seatName};

// ---- registry ---------------------------------------------------------------------------------------------------

void registryGlobal(void* data, wl_registry* registry, uint32_t name, const char* interface, uint32_t version) {
    auto& c = *static_cast<Client*>(data);
    if (std::strcmp(interface, wl_compositor_interface.name) == 0) {
        c.compositorVersion = std::min<uint32_t>(version, 6);
        c.compositor = static_cast<wl_compositor*>(wl_registry_bind(registry, name, &wl_compositor_interface, c.compositorVersion));
    } else if (std::strcmp(interface, wl_subcompositor_interface.name) == 0) {
        c.subcompositor = static_cast<wl_subcompositor*>(wl_registry_bind(registry, name, &wl_subcompositor_interface, 1));
    } else if (std::strcmp(interface, wl_output_interface.name) == 0) {
        wl_registry_bind(registry, name, &wl_output_interface, 1); // U0 spike: so that wl_surface.enter/leave are sent
    } else if (std::strcmp(interface, wl_shm_interface.name) == 0) {
        c.shm = static_cast<wl_shm*>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
    } else if (std::strcmp(interface, xdg_wm_base_interface.name) == 0) {
        c.wmBase = static_cast<xdg_wm_base*>(wl_registry_bind(registry, name, &xdg_wm_base_interface, std::min<uint32_t>(version, 6)));
        xdg_wm_base_add_listener(c.wmBase, &kWmBaseListener, &c);
    } else if (std::strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        c.layerShell = static_cast<zwlr_layer_shell_v1*>(
            wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, std::min<uint32_t>(version, 4)));
    } else if (std::strcmp(interface, wl_seat_interface.name) == 0 && !c.seat) {
        c.seat = static_cast<wl_seat*>(wl_registry_bind(registry, name, &wl_seat_interface, std::min<uint32_t>(version, 9)));
        wl_seat_add_listener(c.seat, &kSeatListener, &c);
    }
}
void registryRemove(void*, wl_registry*, uint32_t) {}
const wl_registry_listener kRegistryListener = {.global = registryGlobal, .global_remove = registryRemove};

// ---- control ----------------------------------------------------------------------------------------------------

void handleCommand(Client& c, const std::string& line) {
    int      x = 0, y = 0, w = 0, h = 0;
    unsigned rgb = 0;
    if (std::sscanf(line.c_str(), "fill %d %d %d %d %6x", &x, &y, &w, &h, &rgb) == 5 && w > 0 && h > 0) {
        paintRect(c, x, y, w, h, rgb & 0xFFFFFF);
        commit(c, true, x, y, w, h);
    } else if (line == "redraw") {
        redraw(c);
    } else if (line == "sync") {
        commit(c, false, 0, 0, 0, 0);
    } else if (line == "quit") {
        c.running = false;
    } else if (!line.empty()) {
        logLine(c, "\"ev\": \"error\", \"message\": \"unknown control command\"");
    }
}

Options parseArgs(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> const char* {
            if (i + 1 >= argc)
                die("missing value for " + a);
            return argv[++i];
        };
        if (a == "--size") {
            if (std::sscanf(next(), "%dx%d", &o.width, &o.height) != 2 || o.width < 2 * kMarker || o.height < 2 * kMarker)
                die("--size must be WxH with W,H >= 32");
        } else if (a == "--title") {
            o.title = next();
        } else if (a == "--app-id") {
            o.appId = next();
        } else if (a == "--log") {
            o.logPath = next();
        } else if (a == "--control") {
            o.controlPath = next();
        } else if (a == "--layer") {
            o.layer = next();
            if (o.layer != "TOP" && o.layer != "OVERLAY")
                die("--layer must be TOP or OVERLAY");
        } else if (a == "--position") {
            if (std::sscanf(next(), "%d,%d", &o.posX, &o.posY) != 2)
                die("--position must be X,Y");
        } else if (a == "--scale-aware") {
            o.scaleAware = true;
        } else if (a == "--resizable") {
            o.resizable = true;
        } else if (a == "--popup" || a == "--subsurface") {
            Rect& r = a == "--popup" ? o.popup : o.subsurface;
            if (std::sscanf(next(), "%d,%d,%d,%d", &r.x, &r.y, &r.w, &r.h) != 4 || r.w < 2 * kMarker || r.h < 2 * kMarker)
                die(a + " must be X,Y,W,H with W,H >= 32");
            r.set = true;
        } else {
            die("unknown argument " + a);
        }
    }
    if (o.logPath.empty())
        die("--log is required");
    if (!o.layer.empty() && (o.popup.set || o.subsurface.set))
        die("--popup and --subsurface need a toplevel (no --layer)");
    return o;
}

} // namespace

int main(int argc, char** argv) {
    Client c;
    c.opts = parseArgs(argc, argv);
    c.popup = {.name = "popup", .rect = c.opts.popup, .palette = &kPopupPalette};
    c.sub = {.name = "sub", .rect = c.opts.subsurface, .palette = &kSubPalette};
    c.log = std::fopen(c.opts.logPath.c_str(), "a");
    if (!c.log)
        die("cannot open log " + c.opts.logPath);

    c.display = wl_display_connect(nullptr);
    if (!c.display)
        die("cannot connect to WAYLAND_DISPLAY");
    wl_registry* registry = wl_display_get_registry(c.display);
    wl_registry_add_listener(registry, &kRegistryListener, &c);
    wl_display_roundtrip(c.display);
    if (!c.compositor || !c.shm)
        die("compositor lacks wl_compositor or wl_shm");

    createBuffers(c);
    c.surface = wl_compositor_create_surface(c.compositor);
    if (c.compositorVersion >= 6)
        wl_surface_add_listener(c.surface, &kSurfaceListener, &c);

    if (c.opts.layer.empty()) {
        if (!c.wmBase)
            die("compositor lacks xdg_wm_base");
        c.xdgSurface = xdg_wm_base_get_xdg_surface(c.wmBase, c.surface);
        xdg_surface_add_listener(c.xdgSurface, &kXdgSurfaceListener, &c);
        c.toplevel = xdg_surface_get_toplevel(c.xdgSurface);
        xdg_toplevel_add_listener(c.toplevel, &kToplevelListener, &c);
        xdg_toplevel_set_title(c.toplevel, c.opts.title.c_str());
        xdg_toplevel_set_app_id(c.toplevel, c.opts.appId.c_str());
        if (!c.opts.resizable) {
            xdg_toplevel_set_min_size(c.toplevel, c.opts.width, c.opts.height);
            xdg_toplevel_set_max_size(c.toplevel, c.opts.width, c.opts.height);
        }
    } else {
        if (!c.layerShell)
            die("compositor lacks zwlr_layer_shell_v1");
        const auto layer = c.opts.layer == "TOP" ? ZWLR_LAYER_SHELL_V1_LAYER_TOP : ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY;
        c.layerSurface = zwlr_layer_shell_v1_get_layer_surface(c.layerShell, c.surface, nullptr, layer, c.opts.appId.c_str());
        zwlr_layer_surface_v1_add_listener(c.layerSurface, &kLayerListener, &c);
        zwlr_layer_surface_v1_set_size(c.layerSurface, c.opts.width, c.opts.height);
        zwlr_layer_surface_v1_set_anchor(c.layerSurface, ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
        zwlr_layer_surface_v1_set_margin(c.layerSurface, c.opts.posY, 0, 0, c.opts.posX);
        zwlr_layer_surface_v1_set_exclusive_zone(c.layerSurface, -1);
    }
    wl_surface_commit(c.surface);

    int control = -1;
    if (!c.opts.controlPath.empty()) {
        // O_RDWR keeps the FIFO open while writers come and go, so reads never see EOF.
        control = open(c.opts.controlPath.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
        if (control < 0)
            die("cannot open control FIFO " + c.opts.controlPath);
    }

    std::string pending;
    while (c.running) {
        while (wl_display_prepare_read(c.display) != 0)
            wl_display_dispatch_pending(c.display);
        wl_display_flush(c.display);
        pollfd fds[2] = {{wl_display_get_fd(c.display), POLLIN, 0}, {control, POLLIN, 0}};
        if (poll(fds, control >= 0 ? 2 : 1, -1) < 0) {
            wl_display_cancel_read(c.display);
            if (errno == EINTR)
                continue;
            die("poll failed");
        }
        if (fds[0].revents & POLLIN) {
            if (wl_display_read_events(c.display) < 0)
                die("display lost");
        } else {
            wl_display_cancel_read(c.display);
        }
        if (fds[0].revents & (POLLERR | POLLHUP))
            die("display lost");
        if (wl_display_dispatch_pending(c.display) < 0)
            die("display lost");
        if (control >= 0 && (fds[1].revents & POLLIN)) {
            char    chunk[512];
            ssize_t n;
            while ((n = read(control, chunk, sizeof chunk)) > 0)
                pending.append(chunk, static_cast<size_t>(n));
            size_t newline;
            while ((newline = pending.find('\n')) != std::string::npos) {
                handleCommand(c, pending.substr(0, newline));
                pending.erase(0, newline + 1);
            }
        }
    }

    logLine(c, "\"ev\": \"quit\"");
    wl_display_flush(c.display);
    wl_display_disconnect(c.display);
    std::fclose(c.log);
    return 0;
}
