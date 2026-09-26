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
        if (!splitFirst(payload, className, title)) {
            return std::nullopt;
        }
        return ActiveWindowEvent{std::string(className), std::string(title)};
    }

    if (name == "activewindowv2") {
        if (payload.empty()) {
            return std::nullopt;
        }
        return ActiveWindowV2Event{std::string(payload)};
    }

    if (name == "activelayout") {
        std::string_view keyboardName;
        std::string_view layoutName;
        if (!splitFirst(payload, keyboardName, layoutName)) {
            return std::nullopt;
        }
        return ActiveLayoutEvent{std::string(keyboardName), std::string(layoutName)};
    }

    if (name == "changefloatingmode") {
        std::string_view floatingValue;
        std::string_view address;
        if (!splitFirst(payload, floatingValue, address) || address.empty()) {
            return std::nullopt;
        }
        if (floatingValue == "0") {
            return ChangeFloatingModeEvent{false, std::string(address)};
        }
        if (floatingValue == "1") {
            return ChangeFloatingModeEvent{true, std::string(address)};
        }
        return std::nullopt;
    }

    if (name == "workspace") {
        return WorkspaceEvent{std::string(payload)};
    }

    if (name == "workspacev2") {
        std::string_view idValue;
        std::string_view workspaceName;
        if (!splitFirst(payload, idValue, workspaceName)) {
            return std::nullopt;
        }
        const auto id = parseInteger(idValue);
        if (!id) {
            return std::nullopt;
        }
        return WorkspaceV2Event{*id, std::string(workspaceName)};
    }

    if (name == "windowtitle") {
        if (payload.empty()) {
            return std::nullopt;
        }
        return WindowTitleEvent{std::string(payload)};
    }

    if (name == "windowtitlev2") {
        std::string_view address;
        std::string_view title;
        if (!splitFirst(payload, address, title) || address.empty()) {
            return std::nullopt;
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
            address.empty() || workspace.empty() || className.empty()) {
            return unknown();
        }
        return OpenWindowEvent{
            std::string(address),
            std::string(workspace),
            std::string(className),
            std::string(title)};
    }

    if (name == "closewindow") {
        if (payload.empty() || payload.find(',') != std::string_view::npos) {
            return unknown();
        }
        return CloseWindowEvent{std::string(payload)};
    }

    return unknown();
}

} // namespace hyprvx::hyprland
