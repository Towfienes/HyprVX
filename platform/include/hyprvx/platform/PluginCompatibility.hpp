#pragma once

#include <string_view>

namespace hyprvx::platform {

inline constexpr std::string_view SupportedHyprlandCommit =
    "efb50993780079460b0cbed1363e2166a2de1d9f";

[[nodiscard]] constexpr bool pluginCommitCompatible(std::string_view compiled,
                                                     std::string_view running) noexcept {
    return compiled == SupportedHyprlandCommit && running == SupportedHyprlandCommit;
}

} // namespace hyprvx::platform
