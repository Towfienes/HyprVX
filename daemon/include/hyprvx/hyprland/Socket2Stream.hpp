#pragma once

#include <filesystem>
#include <optional>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace hyprvx::hyprland {

class LineFramer {
public:
    static constexpr std::size_t MaximumLineBytes = 64 * 1024;

    [[nodiscard]] std::vector<std::string> feed(std::string_view bytes);
    [[nodiscard]] std::optional<std::string> finish();

private:
    std::string pending_;
};

[[nodiscard]] std::filesystem::path resolveSocketPath(
    std::string_view signature,
    const std::optional<std::filesystem::path>& runtimeDirectory,
    const std::filesystem::path& temporaryHyprDirectory = "/tmp/hypr");

class Socket2Stream {
public:
    Socket2Stream();
    ~Socket2Stream();

    Socket2Stream(const Socket2Stream&) = delete;
    Socket2Stream& operator=(const Socket2Stream&) = delete;

    [[nodiscard]] std::optional<std::string> nextLine();
    [[nodiscard]] const std::filesystem::path& socketPath() const { return socketPath_; }

private:
    int socketFd_ = -1;
    bool finished_ = false;
    LineFramer framer_;
    std::vector<std::string> readyLines_;
    std::size_t nextReadyLine_ = 0;
    std::filesystem::path socketPath_;
};

} // namespace hyprvx::hyprland
