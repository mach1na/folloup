#include "settings_todos_page_coordinator.h"

#include <cstdio>
#include <string>

namespace {

std::string FormatArchiveAfterLabel(int days)
{
    if (days <= 0) {
        return "Never";
    }
    char buffer[24] = {};
    std::snprintf(buffer, sizeof(buffer), "%d days", days);
    return std::string(buffer);
}

}  // namespace

SettingsTodosPageCoordinator::SettingsTodosPageCoordinator() = default;

void SettingsTodosPageCoordinator::Show()
{
    focus_.Configure(navigation_model_.item_count, 0);
}

bool SettingsTodosPageCoordinator::MoveFocus(int delta)
{
    return focus_.Move(delta);
}

bool SettingsTodosPageCoordinator::SetFocusIndex(int index)
{
    return focus_.SetIndex(index);
}

bool SettingsTodosPageCoordinator::IsRoleFocused(page_navigation::NavigationItemRole role) const
{
    return navigation_model_.IsRoleSelected(focus_.index(), role);
}

epaper_ui::SettingsTodosPageState SettingsTodosPageCoordinator::BuildState(
    int archive_after_days) const
{
    epaper_ui::SettingsTodosPageState state = {};
    state.navigation_focus_index = focus_.index();
    state.archive_after_input = {
        .label_text = "Archive todos after",
        .value_text = FormatArchiveAfterLabel(archive_after_days),
        .focused = IsRoleFocused(
            page_navigation::NavigationItemRole::kSettingsTodosArchiveAfterInput),
    };
    state.back = {
        .label_text = "Back",
        .selected =
            IsRoleFocused(page_navigation::NavigationItemRole::kSettingsTodosBackButton),
    };
    return state;
}
