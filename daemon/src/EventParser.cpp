#include "hyprvx/hyprland/EventParser.hpp"

#include <charconv>
#include <string_view>

namespace hyprvx::hyprland {
namespace {

bool isTimestamp(std::string_view value) {
    if (value.size() != 8 || value[2] != ':' || value[5] != ':') {
        return false;
    }

    for (std::size_t index = 0; index < value.size(); ++index) {
        if (index == 2 || index == 5) {
            continue;
        }
        if (value[index] < '0' || value[index] > '9') {
            return false;
        }
    }
    return true;
}

bool isValidEventName(std::string_view name) {
    if (name.empty()) {
        return false;
    }
    for (const char character : name) {
        const bool valid =
            (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') ||
            character == '_';
        if (!valid) {
            return false;
        }
    }
    return true;
}

bool isValidAddress(std::string_view address) {
    if (address.starts_with("0x") || address.starts_with("0X")) {
        address.remove_prefix(2);
    }
    if (address.empty()) {
        return false;
    }
    for (const char character : address) {
        const bool isHex =
            (character >= '0' && character <= '9') ||
            (character >= 'a' && character <= 'f') ||
            (character >= 'A' && character <= 'F');
        if (!isHex) {
            return false;
        }
    }
    return true;
}

bool splitFirst(std::string_view value, std::string_view& first, std::string_view& rest) {
    const auto delimiter = value.find(',');
    if (delimiter == std::string_view::npos) {
        return false;
    }
    first = value.substr(0, delimiter);
    rest = value.substr(delimiter + 1);
    return true;
}

std::optional<int> parseInteger(std::string_view value) {
    if (value.empty()) {
        return std::nullopt;
    }
    int parsed = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
        return std::nullopt;
    }
    return parsed;
}

} // namespace

std::optional<Event> EventParser::parse(std::string_view line) {
    if (!line.empty() && line.front() == '[') {
        const auto closing = line.find("] ");
        if (closing == std::string_view::npos || !isTimestamp(line.substr(1, closing - 1))) {
            return std::nullopt;
        }
        line.remove_prefix(closing + 2);
    }

    const auto delimiter = line.find(">>");
    if (delimiter == std::string_view::npos) {
        return std::nullopt;
    }

    const auto name = line.substr(0, delimiter);
    const auto payload = line.substr(delimiter + 2);
    if (!isValidEventName(name)) {
        return std::nullopt;
    }

    const auto unknown = [&]() -> std::optional<Event> {
        return UnknownEvent{std::string(name), std::string(payload)};
    };

    if (name == "activewindow") {
        std::string_view className;
        std::string_view title;
        if (!splitFirst(payload, className, title) || className.empty()) {
            return unknown();
        }
        return ActiveWindowEvent{std::string(className), std::string(title)};
    }

    if (name == "activewindowv2") {
        if (payload.empty()) {
            return ActiveWindowV2Event{std::nullopt};
        }
        if (!isValidAddress(payload)) {
            return unknown();
        }
        return ActiveWindowV2Event{std::string(payload)};
    }

    if (name == "activelayout") {
        std::string_view keyboardName;
        std::string_view layoutName;
        if (!splitFirst(payload, keyboardName, layoutName) || keyboardName.empty() || layoutName.empty()) {
            return unknown();
        }
        return ActiveLayoutEvent{std::string(keyboardName), std::string(layoutName)};
    }

    if (name == "changefloatingmode") {
        std::string_view address;
        std::string_view floatingValue;
        if (!splitFirst(payload, address, floatingValue) || !isValidAddress(address) || floatingValue.empty() ||
            floatingValue.find(',') != std::string_view::npos) {
            return unknown();
        }
        if (floatingValue == "0") {
            return ChangeFloatingModeEvent{false, std::string(address)};
        }
        if (floatingValue == "1") {
            return ChangeFloatingModeEvent{true, std::string(address)};
        }
        return unknown();
    }

    if (name == "workspace") {
        if (payload.empty()) {
            return unknown();
        }
        return WorkspaceEvent{std::string(payload)};
    }

    if (name == "workspacev2") {
        std::string_view idValue;
        std::string_view workspaceName;
        if (!splitFirst(payload, idValue, workspaceName) || workspaceName.empty()) {
            return unknown();
        }
        const auto id = parseInteger(idValue);
        if (!id) {
            return unknown();
        }
        return WorkspaceV2Event{*id, std::string(workspaceName)};
    }

    if (name == "windowtitle") {
        if (!isValidAddress(payload)) {
            return unknown();
        }
        return WindowTitleEvent{std::string(payload)};
    }

    if (name == "windowtitlev2") {
        std::string_view address;
        std::string_view title;
        if (!splitFirst(payload, address, title) || !isValidAddress(address)) {
            return unknown();
        }
        return WindowTitleV2Event{std::string(address), std::string(title)};
    }

    if (name == "openwindow") {
        std::string_view address;
        std::string_view remainder;
        std::string_view workspace;
        std::string_view remainderAfterWorkspace;
        std::string_view className;
        std::string_view title;
        if (!splitFirst(payload, address, remainder) ||
            !splitFirst(remainder, workspace, remainderAfterWorkspace) ||
            !splitFirst(remainderAfterWorkspace, className, title) ||
            !isValidAddress(address) || workspace.empty() || className.empty()) {
            return unknown();
        }
        return OpenWindowEvent{
            std::string(address),
            std::string(workspace),
            std::string(className),
            std::string(title)};
    }

    if (name == "closewindow") {
        if (!isValidAddress(payload)) {
            return unknown();
        }
        return CloseWindowEvent{std::string(payload)};
    }

    if (name == "movewindow") {
        std::string_view address;
        std::string_view workspace;
        if (!splitFirst(payload, address, workspace) || !isValidAddress(address) || workspace.empty()) {
            return unknown();
        }
        return MoveWindowEvent{std::string(address), std::string(workspace)};
    }

    if (name == "movewindowv2") {
        std::string_view address;
        std::string_view remainder;
        std::string_view workspaceIdValue;
        std::string_view workspaceName;
        if (!splitFirst(payload, address, remainder) ||
            !splitFirst(remainder, workspaceIdValue, workspaceName) ||
            !isValidAddress(address) || workspaceName.empty()) {
            return unknown();
        }
        const auto workspaceId = parseInteger(workspaceIdValue);
        if (!workspaceId) {
            return unknown();
        }
        return MoveWindowV2Event{
            std::string(address), *workspaceId, std::string(workspaceName)};
    }

    if (name == "createworkspace") {
        if (payload.empty() || payload.find(',') != std::string_view::npos) {
            return unknown();
        }
        return CreateWorkspaceEvent{std::string(payload)};
    }

    if (name == "createworkspacev2" || name == "destroyworkspacev2") {
        std::string_view workspaceIdValue;
        std::string_view workspaceName;
        if (!splitFirst(payload, workspaceIdValue, workspaceName) || workspaceName.empty()) {
            return unknown();
        }
        const auto workspaceId = parseInteger(workspaceIdValue);
        if (!workspaceId) {
            return unknown();
        }
        if (name == "createworkspacev2") {
            return CreateWorkspaceV2Event{*workspaceId, std::string(workspaceName)};
        }
        return DestroyWorkspaceV2Event{*workspaceId, std::string(workspaceName)};
    }

    if (name == "destroyworkspace") {
        if (payload.empty() || payload.find(',') != std::string_view::npos) {
            return unknown();
        }
        return DestroyWorkspaceEvent{std::string(payload)};
    }

    return unknown();
}

} // namespace hyprvx::hyprland
