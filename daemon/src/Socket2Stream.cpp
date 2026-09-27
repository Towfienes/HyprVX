#include "hyprvx/hyprland/Socket2Stream.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace hyprvx::hyprland {

std::vector<std::string> LineFramer::feed(std::string_view bytes) {
    std::vector<std::string> lines;
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const auto newline = bytes.find('\n', offset);
        const auto end = newline == std::string_view::npos ? bytes.size() : newline;
        const auto segment = bytes.substr(offset, end - offset);
        if (pending_.size() + segment.size() > MaximumLineBytes) {
            throw std::length_error("socket2 event line exceeds 64 KiB limit");
        }
        pending_.append(segment);
        if (newline == std::string_view::npos) {
            break;
        }

        std::string line = std::move(pending_);
        pending_.clear();
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(std::move(line));
        offset = newline + 1;
    }
    return lines;
}

std::optional<std::string> LineFramer::finish() {
    if (pending_.empty()) {
        return std::nullopt;
    }
    std::string trailing = std::move(pending_);
    pending_.clear();
    if (!trailing.empty() && trailing.back() == '\r') {
        trailing.pop_back();
    }
    return trailing;
}

std::filesystem::path resolveSocketPath(
    std::string_view signature,
    const std::optional<std::filesystem::path>& runtimeDirectory,
    const std::filesystem::path& temporaryHyprDirectory) {
    if (signature.empty()) {
        throw std::runtime_error("HYPRLAND_INSTANCE_SIGNATURE is not set");
    }

    const std::filesystem::path instance(signature);
    if (runtimeDirectory) {
        const auto runtimeSocket = *runtimeDirectory / "hypr" / instance / ".socket2.sock";
        std::error_code error;
        if (std::filesystem::exists(runtimeSocket, error) && !error) {
            return runtimeSocket;
        }
    }

    const auto temporarySocket = temporaryHyprDirectory / instance / ".socket2.sock";
    std::error_code error;
    if (std::filesystem::exists(temporarySocket, error) && !error) {
        return temporarySocket;
    }

    std::string message = "Hyprland socket2 socket not found";
    if (runtimeDirectory) {
        message += " (checked " +
            (*runtimeDirectory / "hypr" / instance / ".socket2.sock").string() + ")";
    }
    message += " (checked " + temporarySocket.string() + ")";
    throw std::runtime_error(message);
}

Socket2Stream::Socket2Stream() {
    const char* signature = std::getenv("HYPRLAND_INSTANCE_SIGNATURE");
    if (signature == nullptr || *signature == '\0') {
        throw std::runtime_error("HYPRLAND_INSTANCE_SIGNATURE is not set");
    }

    std::optional<std::filesystem::path> runtimeDirectory;
    if (const char* runtime = std::getenv("XDG_RUNTIME_DIR"); runtime != nullptr && *runtime != '\0') {
        runtimeDirectory = runtime;
    }
    socketPath_ = resolveSocketPath(signature, runtimeDirectory);

    if (socketPath_.string().size() >= sizeof(sockaddr_un::sun_path)) {
        throw std::runtime_error("Hyprland socket path is too long: " + socketPath_.string());
    }

    socketFd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (socketFd_ < 0) {
        throw std::runtime_error("Could not create socket2 client: " + std::string(std::strerror(errno)));
    }

    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    const auto socketString = socketPath_.string();
    std::copy(socketString.begin(), socketString.end(), address.sun_path);
    address.sun_path[socketString.size()] = '\0';

    if (::connect(socketFd_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) < 0) {
        const std::string error = std::strerror(errno);
        ::close(socketFd_);
        socketFd_ = -1;
        throw std::runtime_error("Could not connect to " + socketPath_.string() + ": " + error);
    }
}

Socket2Stream::~Socket2Stream() {
    if (socketFd_ >= 0) {
        ::close(socketFd_);
    }
}

std::optional<std::string> Socket2Stream::nextLine() {
    if (nextReadyLine_ < readyLines_.size()) {
        return std::move(readyLines_[nextReadyLine_++]);
    }
    readyLines_.clear();
    nextReadyLine_ = 0;

    while (!finished_) {
        char bytes[4096];
        const auto received = ::recv(socketFd_, bytes, sizeof(bytes), 0);
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw std::runtime_error("socket2 read failed: " + std::string(std::strerror(errno)));
        }
        if (received == 0) {
            finished_ = true;
            if (auto trailing = framer_.finish()) {
                return trailing;
            }
            return std::nullopt;
        }

        readyLines_ = framer_.feed(std::string_view(bytes, static_cast<std::size_t>(received)));
        if (!readyLines_.empty()) {
            return std::move(readyLines_[nextReadyLine_++]);
        }
    }
    return std::nullopt;
}

} // namespace hyprvx::hyprland
