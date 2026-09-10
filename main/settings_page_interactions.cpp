#include "settings_page_interactions.h"

namespace settings_page_interactions {
namespace {

using page_navigation::NavigationItemRole;

}  // namespace

ActivateResult HandlePrimaryActivate(const SettingsPageCoordinator& coordinator)
{
    ActivateResult result = {};
    result.handled = true;
    result.play_activate_cue = true;

    if (coordinator.IsRoleFocused(NavigationItemRole::kSettingsMenuNetwork)) {
        result.intent = ActivateIntent::kShowNetwork;
    } else if (coordinator.IsRoleFocused(NavigationItemRole::kSettingsMenuTime)) {
        result.intent = ActivateIntent::kShowTime;
    } else if (coordinator.IsRoleFocused(NavigationItemRole::kSettingsMenuStorage)) {
        result.intent = ActivateIntent::kShowStorage;
    } else if (coordinator.IsRoleFocused(NavigationItemRole::kSettingsMenuTodos)) {
        result.intent = ActivateIntent::kShowTodos;
    } else if (coordinator.IsRoleFocused(NavigationItemRole::kSettingsMenuTopics)) {
        result.intent = ActivateIntent::kShowTopics;
    } else if (coordinator.IsRoleFocused(NavigationItemRole::kSettingsManualOnboardingButton)) {
        result.intent = ActivateIntent::kShowOnboarding;
    } else if (coordinator.IsRoleFocused(NavigationItemRole::kFooterHome)) {
        result.intent = ActivateIntent::kShowHome;
    } else {
        result.handled = false;
        result.play_activate_cue = false;
    }
    return result;
}

void ApplyPrimaryActivateResult(const ActivateResult& result, const ActivateCallbacks& callbacks)
{
    switch (result.intent) {
        case ActivateIntent::kShowHome:
            if (callbacks.show_home) {
                callbacks.show_home();
            }
            break;
        case ActivateIntent::kShowNetwork:
            if (callbacks.show_network) {
                callbacks.show_network();
            }
            break;
        case ActivateIntent::kShowTime:
            if (callbacks.show_time) {
                callbacks.show_time();
            }
            break;
        case ActivateIntent::kShowStorage:
            if (callbacks.show_storage) {
                callbacks.show_storage();
            }
            break;
        case ActivateIntent::kShowTodos:
            if (callbacks.show_todos) {
                callbacks.show_todos();
            }
            break;
        case ActivateIntent::kShowTopics:
            if (callbacks.show_topics) {
                callbacks.show_topics();
            }
            break;
        case ActivateIntent::kShowOnboarding:
            if (callbacks.show_onboarding) {
                callbacks.show_onboarding();
            }
            break;
        case ActivateIntent::kNone:
        default:
            break;
    }
}

FocusMoveResult HandleMoveFocus(SettingsPageCoordinator& coordinator, int delta)
{
    FocusMoveResult result = {};
    if (!coordinator.MoveFocus(delta)) {
        return result;
    }
    result.handled = true;
    result.play_navigation_cue = true;
    result.apply_page_state = true;
    return result;
}

}  // namespace settings_page_interactions
