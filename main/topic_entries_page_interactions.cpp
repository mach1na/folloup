#include "topic_entries_page_interactions.h"

namespace topic_entries_page_interactions {
namespace {

using page_navigation::NavigationItemRole;

}  // namespace

ActivateResult HandlePrimaryActivate(TopicEntriesPageCoordinator& coordinator)
{
    ActivateResult result = {};

    if (coordinator.IsRoleFocused(NavigationItemRole::kTopicEntriesTimelineGroup)) {
        result.handled = true;
        if (coordinator.item_list_active()) {
            result.intent = ActivateIntent::kViewDetails;
            result.play_activate_cue = true;
        } else if (coordinator.EnterFocusedGroup()) {
            result.play_activate_cue = true;
            result.apply_page_state = true;
        }
        return result;
    }

    if (coordinator.IsRoleFocused(NavigationItemRole::kTopicEntriesBackButton)) {
        result.handled = true;
        result.play_activate_cue = true;
        result.intent = ActivateIntent::kShowBack;
        return result;
    }

    if (coordinator.IsRoleFocused(NavigationItemRole::kTopicEntriesSummarizeButton)) {
        result.handled = true;
        result.play_activate_cue = true;
        result.intent = ActivateIntent::kSummarize;
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
        case ActivateIntent::kShowBack:
            if (callbacks.show_back) {
                callbacks.show_back();
            }
            break;
        case ActivateIntent::kViewDetails:
            if (callbacks.view_details) {
                callbacks.view_details();
            }
            break;
        case ActivateIntent::kSummarize:
            if (callbacks.summarize) {
                callbacks.summarize();
            }
            break;
        case ActivateIntent::kNone:
        default:
            break;
    }
}

FocusMoveResult HandleMoveFocus(TopicEntriesPageCoordinator& coordinator, int delta)
{
    FocusMoveResult result = {};
    if (!coordinator.MoveFocus(delta)) {
        return result;
    }
    result.handled = true;
    result.play_navigation_cue = true;
    result.apply_page_state = true;
    result.sync_footer_projection = true;
    return result;
}

}  // namespace topic_entries_page_interactions
