#pragma once

// Nested-only guard shared by the HyprVX test tools (hyprvx-vpointer, hyprvx-vkeyboard, hyprvx-x11client;
// contracts/test-tooling.md 1.3 §4a and 1.4 §4b/§4c). A tool is started by `scripts/nested.sh run`, which sets
// XDG_RUNTIME_DIR to the verified private /tmp/hvx-* directory. The guard refuses (exit 3) unless that directory is
// private and the compositor serving the connection is an agent-prefix Hyprland build under ~/.cache/hyprvx/*/prefix/.
// It never falls back to another display.

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

namespace hyprvx::tools {

[[noreturn]] inline void refuse(const char* tool, int code, const std::string& message) {
    std::fprintf(stderr, "%s: %s\n", tool, message.c_str());
    std::exit(code);
}

// XDG_RUNTIME_DIR must be a private /tmp/hvx-* directory; WAYLAND_DISPLAY a bare socket name inside it.
inline void checkNestedEnvironment(const char* tool, bool needWayland = true) {
    const char* runtime = std::getenv("XDG_RUNTIME_DIR");
    const char* display = std::getenv("WAYLAND_DISPLAY");
    if (!runtime || std::strncmp(runtime, "/tmp/hvx-", 9) != 0 || std::strchr(runtime + 5, '/'))
        refuse(tool, 3, "refusing: XDG_RUNTIME_DIR is not a private /tmp/hvx-* directory (use scripts/nested.sh run)");
    struct stat st{};
    if (lstat(runtime, &st) != 0 || !S_ISDIR(st.st_mode) || st.st_uid != getuid() || (st.st_mode & 0777) != 0700)
        refuse(tool, 3, "refusing: XDG_RUNTIME_DIR is not a directory owned by this user with mode 0700");
    if (needWayland && (!display || !*display || std::strchr(display, '/')))
        refuse(tool, 3, "refusing: WAYLAND_DISPLAY must be a bare socket name inside the nested runtime directory");
}

inline std::string exeOf(pid_t pid) {
    char              exe[PATH_MAX] = {};
    const std::string link = "/proc/" + std::to_string(pid) + "/exe";
    const ssize_t     n = readlink(link.c_str(), exe, sizeof exe - 1);
    return n > 0 ? std::string(exe, static_cast<size_t>(n)) : std::string{};
}

inline pid_t parentOf(pid_t pid) {
    const std::string path = "/proc/" + std::to_string(pid) + "/stat";
    FILE*             f = std::fopen(path.c_str(), "r");
    if (!f)
        return -1;
    char         buf[1024] = {};
    const size_t n = std::fread(buf, 1, sizeof buf - 1, f);
    std::fclose(f);
    const char* close = std::strrchr(buf, ')');
    if (!n || !close)
        return -1;
    char state = 0;
    int  ppid = -1;
    return std::sscanf(close + 1, " %c %d", &state, &ppid) == 2 ? ppid : -1;
}

inline bool isAgentPrefixHyprland(const std::string& path) {
    const char* home = std::getenv("HOME");
    if (!home)
        return false;
    const std::string cacheRoot = std::string(home) + "/.cache/hyprvx/";
    return path.rfind(cacheRoot, 0) == 0 && path.find("/prefix/") != std::string::npos;
}

inline pid_t peerPid(const char* tool, int fd) {
    ucred     cred{};
    socklen_t len = sizeof cred;
    if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) != 0)
        refuse(tool, 3, "refusing: cannot read the server PID (SO_PEERCRED)");
    if (cred.uid != getuid())
        refuse(tool, 3, "refusing: the server runs as another user");
    return cred.pid;
}

// The Wayland connection's peer must be an agent-prefix Hyprland binary.
inline void checkWaylandPeer(const char* tool, int fd) {
    const std::string path = exeOf(peerPid(tool, fd));
    if (!isAgentPrefixHyprland(path))
        refuse(tool, 3, "refusing: the compositor is not an agent-prefix nested build (" + path + ")");
}

// The X11 connection's peer must be the XWayland server of an agent-prefix Hyprland (its direct child), as resolved by
// `nested.sh xdisplay` (test-tooling 1.4 §7).
inline void checkX11Peer(const char* tool, int fd) {
    const pid_t server = peerPid(tool, fd);
    const pid_t parent = parentOf(server);
    if (parent <= 1 || !isAgentPrefixHyprland(exeOf(parent)))
        refuse(tool, 3, "refusing: the X server is not the XWayland of an agent-prefix nested build");
}

} // namespace hyprvx::tools
