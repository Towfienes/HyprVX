#include "hyprvx/hyprland/SemanticEventAdapter.hpp"

namespace hyprvx::hyprland {

std::optional<SemanticEvent> SemanticEventAdapter::adapt(const Event& event) const {
    if (const auto* value = std::get_if<ActiveWindowV2Event>(&event)) {
        if (!value->address) {
            return WindowFocusCleared{};
        }
        return WindowFocused{*value->address};
    }
    if (const auto* value = std::get_if<OpenWindowEvent>(&event)) {
        return WindowOpened{
            value->address,
            value->className,
            value->title,
            value->workspace,
            std::nullopt};
    }
    if (const auto* value = std::get_if<CloseWindowEvent>(&event)) {
        return WindowClosed{value->address};
    }
    if (const auto* value = std::get_if<MoveWindowV2Event>(&event)) {
        return WindowWorkspaceChanged{
            value->address,
            value->workspaceId,
            value->workspaceName};
    }
    if (const auto* value = std::get_if<WindowTitleV2Event>(&event)) {
        return WindowTitleChanged{value->address, value->title};
    }
    if (const auto* value = std::get_if<ChangeFloatingModeEvent>(&event)) {
        return WindowFloatingChanged{value->address, value->floating};
    }
    if (const auto* value = std::get_if<WorkspaceV2Event>(&event)) {
        return WorkspaceFocused{value->id, value->name};
    }
    if (const auto* value = std::get_if<CreateWorkspaceV2Event>(&event)) {
        return WorkspaceCreated{value->workspaceId, value->workspaceName};
    }
    if (const auto* value = std::get_if<DestroyWorkspaceV2Event>(&event)) {
        return WorkspaceDestroyed{value->workspaceId, value->workspaceName};
    }
    return std::nullopt;
}

} // namespace hyprvx::hyprland
