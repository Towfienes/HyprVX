#include "hyprvx/hyprland/RuntimeMirror.hpp"

#include <algorithm>
#include <cctype>
#include <type_traits>

namespace hyprvx::hyprland {

std::string canonicalWindowAddress(std::string_view address) {
    if (address.starts_with("0x") || address.starts_with("0X")) {
        address.remove_prefix(2);
    }
    if (address.empty()) {
        return {};
    }

    const auto isHexDigit = [](char value) {
        return (value >= '0' && value <= '9') ||
            (value >= 'a' && value <= 'f') ||
            (value >= 'A' && value <= 'F');
    };
    if (!std::all_of(address.begin(), address.end(), isHexDigit)) {
        return std::string(address);
    }

    std::string canonical(address);
    std::transform(canonical.begin(), canonical.end(), canonical.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return canonical;
}

ObservedWindowState& RuntimeMirror::ensureWindow(const std::string& address) {
    auto [iterator, inserted] = windows_.try_emplace(address);
    if (inserted) {
        iterator->second.address = address;
    }
    return iterator->second;
}

const ObservedWindowState* RuntimeMirror::findWindow(const std::string& address) const {
    const auto iterator = windows_.find(canonicalWindowAddress(address));
    return iterator == windows_.end() ? nullptr : &iterator->second;
}

bool RuntimeMirror::isRecentlyClosed(const std::string& address) const {
    return recentlyClosedSet_.contains(address);
}

void RuntimeMirror::rememberClosed(const std::string& address) {
    if (recentlyClosedSet_.insert(address).second) {
        recentlyClosedOrder_.push_back(address);
    }
    while (recentlyClosedOrder_.size() > RecentlyClosedLimit) {
        recentlyClosedSet_.erase(recentlyClosedOrder_.front());
        recentlyClosedOrder_.pop_front();
    }
}

void RuntimeMirror::allowReopen(const std::string& address) {
    recentlyClosedSet_.erase(address);
    recentlyClosedOrder_.erase(
        std::remove(recentlyClosedOrder_.begin(), recentlyClosedOrder_.end(), address),
        recentlyClosedOrder_.end());
}

void RuntimeMirror::reset() {
    windows_.clear();
    focusedWindow_.reset();
    focusedWorkspaceId_.reset();
    focusedWorkspaceName_.reset();
    knownWorkspaces_.clear();
    recentlyClosedOrder_.clear();
    recentlyClosedSet_.clear();
}

bool RuntimeMirror::apply(const SemanticEvent& event) {
    return std::visit([this](const auto& value) -> bool {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, WindowFocused>) {
            const auto address = canonicalWindowAddress(value.address);
            if (address.empty() || isRecentlyClosed(address)) {
                return false;
            }
            ensureWindow(address);
            focusedWindow_ = address;
            return true;
        } else if constexpr (std::is_same_v<T, WindowFocusCleared>) {
            focusedWindow_.reset();
            return true;
        } else if constexpr (std::is_same_v<T, WindowOpened>) {
            const auto address = canonicalWindowAddress(value.address);
            if (address.empty()) {
                return false;
            }
            allowReopen(address);
            auto& state = ensureWindow(address);
            if (!state.className) state.className = value.className;
            if (!state.title) state.title = value.title;
            if (!state.workspaceName) state.workspaceName = value.workspaceName;
            if (!state.workspaceId && value.workspaceId) state.workspaceId = value.workspaceId;
            if (value.workspaceId) {
                knownWorkspaces_[*value.workspaceId] = value.workspaceName;
            }
            return true;
        } else if constexpr (std::is_same_v<T, WindowClosed>) {
            const auto address = canonicalWindowAddress(value.address);
            if (address.empty()) {
                return false;
            }
            const bool removedWindow = windows_.erase(address) > 0;
            const bool wasAlreadyClosed = isRecentlyClosed(address);
            rememberClosed(address);
            bool clearedFocus = false;
            if (focusedWindow_ && *focusedWindow_ == address) {
                focusedWindow_.reset();
                clearedFocus = true;
            }
            return removedWindow || clearedFocus || !wasAlreadyClosed;
        } else if constexpr (std::is_same_v<T, WindowWorkspaceChanged>) {
            const auto address = canonicalWindowAddress(value.address);
            if (address.empty() || isRecentlyClosed(address)) {
                return false;
            }
            auto& state = ensureWindow(address);
            state.workspaceId = value.workspaceId;
            state.workspaceName = value.workspaceName;
            knownWorkspaces_[value.workspaceId] = value.workspaceName;
            return true;
        } else if constexpr (std::is_same_v<T, WindowTitleChanged>) {
            const auto address = canonicalWindowAddress(value.address);
            if (address.empty() || isRecentlyClosed(address)) {
                return false;
            }
            ensureWindow(address).title = value.title;
            return true;
        } else if constexpr (std::is_same_v<T, WindowFloatingChanged>) {
            const auto address = canonicalWindowAddress(value.address);
            if (address.empty() || isRecentlyClosed(address)) {
                return false;
            }
            ensureWindow(address).floating = value.floating;
            return true;
        } else if constexpr (std::is_same_v<T, WorkspaceFocused>) {
            focusedWorkspaceId_ = value.workspaceId;
            focusedWorkspaceName_ = value.workspaceName;
            knownWorkspaces_[value.workspaceId] = value.workspaceName;
            return true;
        } else if constexpr (std::is_same_v<T, WorkspaceCreated>) {
            knownWorkspaces_[value.workspaceId] = value.workspaceName;
            return true;
        } else if constexpr (std::is_same_v<T, WorkspaceDestroyed>) {
            knownWorkspaces_.erase(value.workspaceId);
            if (focusedWorkspaceId_ && *focusedWorkspaceId_ == value.workspaceId) {
                focusedWorkspaceId_.reset();
                focusedWorkspaceName_.reset();
            }
            return true;
        }
    }, event);
}

} // namespace hyprvx::hyprland
