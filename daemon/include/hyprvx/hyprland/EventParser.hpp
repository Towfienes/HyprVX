#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace hyprvx::hyprland {

struct ActiveWindowEvent {
    std::string className;
    std::string title;
};

struct ActiveWindowV2Event {
    std::string address;
};

struct ActiveLayoutEvent {
    std::string keyboardName;
    std::string layoutName;
};

struct ChangeFloatingModeEvent {
    bool floating;
    std::string address;
};

struct WorkspaceEvent {
    std::string name;
};

struct WorkspaceV2Event {
    int id;
    std::string name;
};

struct WindowTitleEvent {
    std::string address;
};

struct WindowTitleV2Event {
    std::string address;
    std::string title;
};

struct OpenWindowEvent {
    std::string address;
    std::string workspace;
    std::string className;
    std::string title;
};

struct CloseWindowEvent {
    std::string address;
};

struct MoveWindowEvent {
    std::string address;
    std::string workspace;
};

struct MoveWindowV2Event {
    std::string address;
    int workspaceId;
    std::string workspaceName;
};

struct CreateWorkspaceEvent {
    std::string workspace;
};

struct CreateWorkspaceV2Event {
    int workspaceId;
    std::string workspaceName;
};

struct DestroyWorkspaceEvent {
    std::string workspace;
};

struct DestroyWorkspaceV2Event {
    int workspaceId;
    std::string workspaceName;
};

struct UnknownEvent {
    std::string name;
    std::string payload;
};

using Event = std::variant<
    ActiveWindowEvent,
    ActiveWindowV2Event,
    ActiveLayoutEvent,
    ChangeFloatingModeEvent,
    WorkspaceEvent,
    WorkspaceV2Event,
    WindowTitleEvent,
    WindowTitleV2Event,
    OpenWindowEvent,
    CloseWindowEvent,
    MoveWindowEvent,
    MoveWindowV2Event,
    CreateWorkspaceEvent,
    CreateWorkspaceV2Event,
    DestroyWorkspaceEvent,
    DestroyWorkspaceV2Event,
    UnknownEvent>;

class EventParser {
public:
    // Accepts raw socket2 lines and [HH:MM:SS]-prefixed probe output.
    // Returns no event when framing is malformed. Malformed recognized payloads
    // are preserved as UnknownEvent so callers can inspect the original data.
    [[nodiscard]] static std::optional<Event> parse(std::string_view line);
};

} // namespace hyprvx::hyprland
