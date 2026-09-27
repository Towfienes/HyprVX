#include "hyprvx/hyprland/SemanticEvent.hpp"

#include <type_traits>
#include <utility>

namespace hyprvx::hyprland {

std::string describe(const SemanticEvent& event) {
    return std::visit([](const auto& value) -> std::string {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, WindowFocused>) {
            return "WindowFocused " + value.address;
        } else if constexpr (std::is_same_v<T, WindowFocusCleared>) {
            return "WindowFocusCleared";
        } else if constexpr (std::is_same_v<T, WindowOpened>) {
            return "WindowOpened " + value.address + " " + value.className +
                " workspace=" + value.workspaceName;
        } else if constexpr (std::is_same_v<T, WindowClosed>) {
            return "WindowClosed " + value.address;
        } else if constexpr (std::is_same_v<T, WindowWorkspaceChanged>) {
            return "WindowMoved " + value.address + " workspace=" + value.workspaceName;
        } else if constexpr (std::is_same_v<T, WindowTitleChanged>) {
            return "WindowTitleChanged " + value.address + " title=" + value.title;
        } else if constexpr (std::is_same_v<T, WindowFloatingChanged>) {
            return "WindowFloatingChanged " + value.address +
                " floating=" + (value.floating ? "true" : "false");
        } else if constexpr (std::is_same_v<T, WorkspaceFocused>) {
            return "WorkspaceFocused " + value.workspaceName;
        } else if constexpr (std::is_same_v<T, WorkspaceCreated>) {
            return "WorkspaceCreated " + value.workspaceName;
        } else {
            return "WorkspaceDestroyed " + value.workspaceName;
        }
    }, event);
}

} // namespace hyprvx::hyprland
