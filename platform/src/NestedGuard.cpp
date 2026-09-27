#include "hyprvx/platform/NestedGuard.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string_view>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

namespace hyprvx::platform {
namespace {

std::string requiredEnvironment(const char* name) {
    const char* value = std::getenv(name);
    if (value == nullptr || *value == '\0') throw std::runtime_error(std::string("missing ") + name);
    return value;
}

bool safeComponent(std::string_view value) noexcept {
    if (value.empty()) return false;
    for (char c : value) {
        if ((c < 'a' || c > 'z') && (c < 'A' || c > 'Z') &&
            (c < '0' || c > '9') && c != '-' && c != '_' && c != '.') return false;
    }
    return value != "." && value != "..";
}

int unixSocketPeer(const std::filesystem::path& path) {
    const std::string name = path.string();
    if (name.size() >= sizeof(sockaddr_un::sun_path)) throw std::runtime_error("nested socket path too long");
    const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) throw std::runtime_error("could not create nested guard socket");
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    std::copy(name.begin(), name.end(), address.sun_path);
    address.sun_path[name.size()] = '\0';
    const bool connected = ::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0;
    struct ucred peer{};
    socklen_t size = sizeof(peer);
    const bool credential = connected && ::getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &peer, &size) == 0;
    ::close(fd);
    if (!credential || size != sizeof(peer) || peer.uid != ::getuid())
        throw std::runtime_error("nested socket peer verification failed: " + name);
    return peer.pid;
}

} // namespace

bool nestedContractMatches(const NestedContract& contract, const NestedEvidence& evidence) noexcept {
    return evidence.markerValue == "1" && safeComponent(contract.signature) &&
        safeComponent(contract.waylandDisplay) && !contract.hostSignature.empty() &&
        contract.signature != contract.hostSignature && contract.compositorPid > 1 &&
        contract.runtimeDirectory.is_absolute() &&
        evidence.signature == contract.signature &&
        evidence.waylandDisplay == contract.waylandDisplay &&
        evidence.runtimeDirectory == contract.runtimeDirectory &&
        evidence.waylandPeerPid == contract.compositorPid &&
        evidence.socketPeerPid == contract.compositorPid;
}

NestedContract requireNestedEnvironment(bool insideCompositor) {
    const std::string markerValue = requiredEnvironment("HYPRVX_NESTED_DEV");
    const auto markerPath = std::filesystem::path(requiredEnvironment("HYPRVX_NESTED_MARKER"));
    struct stat metadata{};
    if (::lstat(markerPath.c_str(), &metadata) != 0 || !S_ISREG(metadata.st_mode) ||
        metadata.st_uid != ::getuid() || (metadata.st_mode & 0077) != 0)
        throw std::runtime_error("nested marker is missing, shared, or unsafe");
    std::ifstream marker(markerPath);
    nlohmann::json data;
    marker >> data;
    NestedContract contract{
        data.at("signature").get<std::string>(),
        data.at("host_signature").get<std::string>(),
        data.at("wayland_display").get<std::string>(),
        data.at("runtime_directory").get<std::string>(),
        data.at("compositor_pid").get<int>()};
    NestedEvidence evidence{
        markerValue,
        requiredEnvironment("HYPRLAND_INSTANCE_SIGNATURE"),
        requiredEnvironment("WAYLAND_DISPLAY"),
        requiredEnvironment("XDG_RUNTIME_DIR"),
        0, 0};
    if (!safeComponent(contract.signature) || !safeComponent(contract.waylandDisplay) ||
        !contract.runtimeDirectory.is_absolute() || contract.compositorPid <= 1)
        throw std::runtime_error("invalid nested marker contract");
    const auto wayland = contract.runtimeDirectory / contract.waylandDisplay;
    const auto socket2 = contract.runtimeDirectory / "hypr" / contract.signature / ".socket2.sock";
    evidence.waylandPeerPid = unixSocketPeer(wayland);
    evidence.socketPeerPid = unixSocketPeer(socket2);
    if (!nestedContractMatches(contract, evidence))
        throw std::runtime_error("nested identity mismatch; refusing host or ambiguous compositor");
    if (insideCompositor && ::getpid() != contract.compositorPid)
        throw std::runtime_error("plugin is not loaded in the guarded nested compositor");
    return contract;
}

} // namespace hyprvx::platform
