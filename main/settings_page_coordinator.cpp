#include "settings_page_coordinator.h"

namespace {

using page_navigation::NavigationItemRole;

// Order must match epaper_ui::SettingsMenuItem / kSettingsMenuLabels.
constexpr NavigationItemRole kMenuRoles[epaper_ui::kSettingsMenuItemCount] = {
    NavigationItemRole::kSettingsMenuNetwork,
    NavigationItemRole::kSettingsMenuTime,
    NavigationItemRole::kSettingsMenuStorage,
    NavigationItemRole::kSettingsMenuTodos,
};

}  // namespace

SettingsPageCoordinator::SettingsPageCoordinator() = default;

void SettingsPageCoordinator::Show()
{
    focus_.Configure(navigation_model_.item_count, 0);
}

bool SettingsPageCoordinator::MoveFocus(int delta)
{
    return focus_.Move(delta);
}

bool SettingsPageCoordinator::SetFocusIndex(int index)
{
    return focus_.SetIndex(index);
}

bool SettingsPageCoordinator::IsRoleFocused(NavigationItemRole role) const
{
    return navigation_model_.IsRoleSelected(focus_.index(), role);
}

epaper_ui::SettingsPageState SettingsPageCoordinator::BuildState() const
{
    epaper_ui::SettingsPageState state = {};
    state.navigation_focus_index = focus_.index();
    state.title_text = "Settings";

    state.menu.selected_index = -1;
    for (int index = 0; index < epaper_ui::kSettingsMenuItemCount; ++index) {
        if (IsRoleFocused(kMenuRoles[index])) {
            state.menu.selected_index = index;
            break;
        }
    }

    state.manual_button = {
        .label_text = "Manual",
        .selected = IsRoleFocused(NavigationItemRole::kSettingsManualOnboardingButton),
    };
    return state;
}
