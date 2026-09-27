#include "hyprvx/hyprland/RuntimeBridge.hpp"
#include "hyprvx/hyprland/Socket2Stream.hpp"
#include "hyprvx/runtime/SpatialProjection.hpp"
#include "hyprvx/runtime/ProjectionStateStore.hpp"

#include <array>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstdlib>

namespace {

struct Options {
    std::string capture;
    std::string state;
    std::string saveState;
    std::string compositorEpoch;
    bool replaceInvalid{};
};

Options parseOptions(int argc, char** argv) {
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string argument(argv[index]);
        auto value = [&]() -> std::string {
            if (++index >= argc) throw std::invalid_argument("missing value for " + argument);
            return argv[index];
        };
        if (argument == "--file") options.capture = value();
        else if (argument == "--state") options.state = value();
        else if (argument == "--save-state") options.saveState = value();
        else if (argument == "--compositor-epoch") options.compositorEpoch = value();
        else if (argument == "--replace-invalid-state") options.replaceInvalid = true;
        else if (argument == "--help") {
            std::cout << "Usage: hyprvx-project-replay --file <capture.log> [--state <state.json>] "
                         "[--save-state <state.json>] [--compositor-epoch <signature>] "
                         "[--replace-invalid-state]\n";
            std::exit(0);
        } else throw std::invalid_argument("unknown option: " + argument);
    }
    if (options.capture.empty()) throw std::invalid_argument("--file is required");
    if (options.compositorEpoch.empty()) {
        if (const char* signature = std::getenv("HYPRLAND_INSTANCE_SIGNATURE"); signature != nullptr && *signature != '\0') {
            options.compositorEpoch = signature;
        } else {
            options.compositorEpoch = "replay-session";
        }
    }
    return options;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parseOptions(argc, argv);
        std::ifstream input(options.capture, std::ios::binary);
        if (!input) throw std::runtime_error("could not open capture file");
        hyprvx::hyprland::RuntimeBridge bridge;
        hyprvx::hyprland::LineFramer framer;
        hyprvx::runtime::SpatialProjector projector;
        hyprvx::World world;
        projector.setCompositorEpoch(options.compositorEpoch);
        hyprvx::runtime::ProjectionStateStore stateStore;
        if (!options.state.empty()) {
            const auto loaded = stateStore.load(options.state);
            if (loaded.state) {
                projector.restoreState(*loaded.state, world);
                if (loaded.status == hyprvx::runtime::StateLoadStatus::RecoveredBackup) {
                    std::cerr << "Recovered projection state from backup: " << loaded.diagnostic << '\n';
                }
            } else if (loaded.status != hyprvx::runtime::StateLoadStatus::Missing) {
                std::cerr << "Ignoring unusable projection state: " << loaded.diagnostic << '\n';
            }
        }
        auto process = [&](std::string_view line) {
            static_cast<void>(bridge.processLine(line));
            projector.reconcile(bridge.mirror(), world);
        };
        std::array<char, 4096> buffer{};
        while (input) {
            input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            const auto count = input.gcount();
            if (count <= 0) break;
            for (const auto& line : framer.feed(std::string_view(buffer.data(), static_cast<std::size_t>(count)))) {
                process(line);
            }
        }
        if (auto tail = framer.finish()) process(*tail);
        if (input.bad()) throw std::runtime_error("failed while reading capture file");

        if (!options.saveState.empty()) {
            stateStore.saveAtomic(options.saveState, projector.snapshot(), options.replaceInvalid);
        }

        std::vector<std::string> workspaceNames;
        workspaceNames.reserve(projector.workspaceMap().regions().size());
        for (const auto& [name, region] : projector.workspaceMap().regions()) {
            static_cast<void>(region);
            workspaceNames.push_back(name);
        }
        std::sort(workspaceNames.begin(), workspaceNames.end());
        for (const auto& name : workspaceNames) {
            const auto* region = projector.workspaceMap().find(name);
            std::cout << "Workspace " << name << " origin=(" << region->origin.x << ',' << region->origin.y << ")\n";
            std::vector<std::string> addresses;
            for (const auto& [address, record] : projector.windows()) {
                if (record.workspaceName == name && record.placementStatus == hyprvx::runtime::PlacementStatus::Projected) {
                    addresses.push_back(address);
                }
            }
            std::sort(addresses.begin(), addresses.end());
            for (const auto& address : addresses) {
                const auto& record = projector.windows().at(address);
                std::cout << "  Window " << address << " id=" << record.windowId << " geometry=("
                          << record.geometry.position.x << ',' << record.geometry.position.y << ','
                          << record.geometry.size.x << ',' << record.geometry.size.y << ")";
                if (record.className) std::cout << " class=" << *record.className;
                if (record.title) std::cout << " title=" << *record.title;
                std::cout << '\n';
            }
        }
        if (const auto focused = projector.focusedWindowId()) std::cout << "focused projected window=" << *focused << '\n';
        else std::cout << "focused projected window=none\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "hyprvx-project-replay: " << error.what() << '\n';
        return std::string(error.what()).starts_with("unknown option:") ||
            std::string(error.what()).find("--file is required") != std::string::npos ? 2 : 1;
    }
}
