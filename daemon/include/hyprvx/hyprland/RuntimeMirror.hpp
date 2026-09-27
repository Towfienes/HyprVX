#pragma once

#include "hyprvx/hyprland/SemanticEvent.hpp"

#include <optional>
#include <deque>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <string_view>

namespace hyprvx::hyprland {

[[nodiscard]] std::string canonicalWindowAddress(std::string_view address);

struct ObservedWindowState {
    std::string address;
    std::optional<std::string> className;
    std::optional<std::string> title;
    std::optional<int> workspaceId;
    std::optional<std::string> workspaceName;
    std::optional<bool> floating;
};

class RuntimeMirror {
public:
    static constexpr std::size_t RecentlyClosedLimit = 256;

    bool apply(const SemanticEvent& event);
    void reset();

    [[nodiscard]] const std::unordered_map<std::string, ObservedWindowState>& windows() const {
        return windows_;
    }
    [[nodiscard]] const ObservedWindowState* findWindow(const std::string& address) const;
    [[nodiscard]] const std::optional<std::string>& focusedWindow() const {
        return focusedWindow_;
    }
    [[nodiscard]] const std::optional<int>& focusedWorkspaceId() const {
        return focusedWorkspaceId_;
    }
    [[nodiscard]] const std::optional<std::string>& focusedWorkspaceName() const {
        return focusedWorkspaceName_;
    }
    [[nodiscard]] const std::unordered_map<int, std::string>& knownWorkspaces() const {
        return knownWorkspaces_;
    }
    [[nodiscard]] std::size_t recentlyClosedCount() const { return recentlyClosedSet_.size(); }

private:
    ObservedWindowState& ensureWindow(const std::string& address);
    [[nodiscard]] bool isRecentlyClosed(const std::string& address) const;
    void rememberClosed(const std::string& address);
    void allowReopen(const std::string& address);

    std::unordered_map<std::string, ObservedWindowState> windows_;
    std::optional<std::string> focusedWindow_;
    std::optional<int> focusedWorkspaceId_;
    std::optional<std::string> focusedWorkspaceName_;
    std::unordered_map<int, std::string> knownWorkspaces_;
    std::deque<std::string> recentlyClosedOrder_;
    std::unordered_set<std::string> recentlyClosedSet_;
};

} // namespace hyprvx::hyprland
