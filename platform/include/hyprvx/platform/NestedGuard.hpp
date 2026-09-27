#pragma once

#include <filesystem>
#include <string>

namespace hyprvx::platform {

struct NestedContract {
    std::string signature;
    std::string hostSignature;
    std::string waylandDisplay;
    std::filesystem::path runtimeDirectory;
    int compositorPid{};
};

struct NestedEvidence {
    std::string markerValue;
    std::string signature;
    std::string waylandDisplay;
    std::filesystem::path runtimeDirectory;
    int waylandPeerPid{};
    int socketPeerPid{};
};

// Pure decision function, shared by the native executable and plugin.
[[nodiscard]] bool nestedContractMatches(const NestedContract& contract,
                                         const NestedEvidence& evidence) noexcept;

// Checks the private launcher marker and the actual peers of both sockets.
// Throws on ambiguity; it never falls back to a host compositor.
[[nodiscard]] NestedContract requireNestedEnvironment(bool insideCompositor = false);

} // namespace hyprvx::platform
