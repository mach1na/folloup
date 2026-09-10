#include "settings_topics_page_interactions.h"

namespace settings_topics_page_interactions {
namespace {

using page_navigation::NavigationItemRole;

}  // namespace

ActivateResult HandlePrimaryActivate(const SettingsTopicsPageCoordinator& coordinator)
{
    ActivateResult result = {};

    if (coordinator.IsRoleFocused(NavigationItemRole::kSettingsTopicsTopicRow)) {
        result.handled = true;
        result.play_activate_cue = true;
        result.intent = ActivateIntent::kOpenTopicActions;
        return result;
    }
    if (coordinator.IsRoleFocused(NavigationItemRole::kSettingsTopicsNewTopicButton)) {
        result.handled = true;
        result.play_activate_cue = true;
        result.intent = ActivateIntent::kNewTopic;
        return result;
    }
    if (coordinator.IsRoleFocused(NavigationItemRole::kSettingsTopicsBackButton)) {
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
        case ActivateIntent::kOpenTopicActions:
            if (callbacks.open_topic_actions) {
                callbacks.open_topic_actions();
            }
            break;
        case ActivateIntent::kNewTopic:
            if (callbacks.new_topic) {
                callbacks.new_topic();
            }
            break;
        case ActivateIntent::kNone:
        default:
            break;
    }
}

FocusMoveResult HandleMoveFocus(SettingsTopicsPageCoordinator& coordinator, int delta)
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

}  // namespace settings_topics_page_interactions
