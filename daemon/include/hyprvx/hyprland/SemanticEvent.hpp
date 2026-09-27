#pragma once

#include <optional>
#include <string>
#include <variant>

namespace hyprvx::hyprland {

struct WindowFocused {
    std::string address;
};

struct WindowFocusCleared {};

struct WindowOpened {
    std::string address;
    std::string className;
    std::string title;
    std::string workspaceName;
    std::optional<int> workspaceId;
};

struct WindowClosed {
    std::string address;
};

struct WindowWorkspaceChanged {
    std::string address;
    int workspaceId;
    std::string workspaceName;
};

struct WindowTitleChanged {
    std::string address;
    std::string title;
};

struct WindowFloatingChanged {
    std::string address;
    bool floating;
};

struct WorkspaceFocused {
    int workspaceId;
    std::string workspaceName;
};

struct WorkspaceCreated {
    int workspaceId;
    std::string workspaceName;
};

struct WorkspaceDestroyed {
    int workspaceId;
    std::string workspaceName;
};

using SemanticEvent = std::variant<
    WindowFocused,
    WindowFocusCleared,
    WindowOpened,
    WindowClosed,
    WindowWorkspaceChanged,
    WindowTitleChanged,
    WindowFloatingChanged,
    WorkspaceFocused,
    WorkspaceCreated,
    WorkspaceDestroyed>;

[[nodiscard]] std::string describe(const SemanticEvent& event);

} // namespace hyprvx::hyprland
