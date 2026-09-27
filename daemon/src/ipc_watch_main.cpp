#include "hyprvx/hyprland/RuntimeBridge.hpp"
#include "hyprvx/hyprland/Socket2Stream.hpp"

#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void report(const std::optional<hyprvx::hyprland::SemanticEvent>& event) {
    if (event) {
        std::cout << hyprvx::hyprland::describe(*event) << '\n' << std::flush;
    }
}

int replay(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Could not open capture file: " + path);
    }

    hyprvx::hyprland::RuntimeBridge bridge;
    hyprvx::hyprland::LineFramer framer;
    std::array<char, 4096> buffer{};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = input.gcount();
        if (count <= 0) {
            break;
        }
        const auto lines = framer.feed(std::string_view(buffer.data(), static_cast<std::size_t>(count)));
        for (const auto& line : lines) {
            report(bridge.processLine(line));
        }
    }
    if (auto trailing = framer.finish()) {
        report(bridge.processLine(*trailing));
    }
    return input.bad() ? 1 : 0;
}

int watch() {
    hyprvx::hyprland::Socket2Stream stream;
    std::cerr << "Watching read-only Hyprland socket2: " << stream.socketPath() << '\n';
    hyprvx::hyprland::RuntimeBridge bridge;
    while (auto line = stream.nextLine()) {
        report(bridge.processLine(*line));
    }
    bridge.endEpoch();
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 1) {
            return watch();
        }
        if (argc == 3 && std::string(argv[1]) == "--file") {
            return replay(argv[2]);
        }
        if (argc == 2 && std::string(argv[1]) == "--help") {
            std::cout << "Usage: hyprvx-ipc-watch [--file <capture.log>]\n"
                         "Without --file, block on the current Hyprland socket2 stream.\n";
            return 0;
        }
        std::cerr << "Usage: hyprvx-ipc-watch [--file <capture.log>]\n";
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "hyprvx-ipc-watch: " << error.what() << '\n';
        return 1;
    }
}
