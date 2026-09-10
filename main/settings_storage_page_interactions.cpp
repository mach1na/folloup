#include "settings_storage_page_interactions.h"

namespace settings_storage_page_interactions {
namespace {

using page_navigation::NavigationItemRole;

}  // namespace

ActivateResult HandlePrimaryActivate(const SettingsStoragePageCoordinator& coordinator)
{
    ActivateResult result = {};

    if (coordinator.IsRoleFocused(NavigationItemRole::kSettingsStorageEnableOtgButton)) {
        result.handled = true;
        result.play_activate_cue = true;
        result.intent = ActivateIntent::kEnableOtg;
        return result;
    }
    if (coordinator.IsRoleFocused(NavigationItemRole::kSettingsStorageFormatSdButton)) {
        result.handled = true;
        result.play_activate_cue = true;
        result.intent = ActivateIntent::kShowFormatSdModal;
        return result;
    }
    if (coordinator.IsRoleFocused(NavigationItemRole::kSettingsStorageBackButton)) {
        // Returns to the Settings hub -- this page is only ever reached from there, so there's
        // no other source to track.
        result.handled = true;
        result.play_activate_cue = true;
        result.intent = ActivateIntent::kShowSettings;
        return result;
    }
    if (coordinator.IsRoleFocused(NavigationItemRole::kFooterHome)) {
        result.handled = true;
        result.play_activate_cue = true;
        result.intent = ActivateIntent::kShowHome;
        return result;
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
        case ActivateIntent::kShowSettings:
            if (callbacks.show_settings) {
                callbacks.show_settings();
            }
            break;
        case ActivateIntent::kEnableOtg:
            if (callbacks.enable_otg) {
                callbacks.enable_otg();
            }
            break;
        case ActivateIntent::kShowFormatSdModal:
            if (callbacks.show_format_sd_modal) {
                callbacks.show_format_sd_modal();
            }
            break;
        case ActivateIntent::kNone:
        default:
            break;
    }
}

FocusMoveResult HandleMoveFocus(SettingsStoragePageCoordinator& coordinator, int delta)
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

}  // namespace settings_storage_page_interactions
