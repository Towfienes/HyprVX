#include "hyprvx/hyprland/RuntimeBridge.hpp"
#include "hyprvx/hyprland/Socket2Stream.hpp"
#include "hyprvx/runtime/SpatialProjection.hpp"

#include <array>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 3 || std::string(argv[1]) != "--file") {
        std::cerr << "Usage: hyprvx-project-replay --file <capture.log>\n";
        return 2;
    }
    try {
        std::ifstream input(argv[2], std::ios::binary);
        if (!input) throw std::runtime_error("could not open capture file");
        hyprvx::hyprland::RuntimeBridge bridge;
        hyprvx::hyprland::LineFramer framer;
        hyprvx::runtime::SpatialProjector projector;
        hyprvx::World world;
        std::array<char, 4096> buffer{};
        while (input) {
            input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            const auto count = input.gcount();
            if (count <= 0) break;
            for (const auto& line : framer.feed(std::string_view(buffer.data(), static_cast<std::size_t>(count)))) {
                static_cast<void>(bridge.processLine(line));
            }
        }
        if (auto tail = framer.finish()) static_cast<void>(bridge.processLine(*tail));
        if (input.bad()) throw std::runtime_error("failed while reading capture file");
        projector.reconcile(bridge.mirror(), world);

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
        return 1;
    }
}
