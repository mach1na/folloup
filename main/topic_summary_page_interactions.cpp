#include "topic_summary_page_interactions.h"

namespace topic_summary_page_interactions {
namespace {

using page_navigation::NavigationItemRole;

}  // namespace

ActivateResult HandlePrimaryActivate(TopicSummaryPageCoordinator& coordinator, bool gemini_ready)
{
    ActivateResult result = {};
    result.handled = true;
    result.play_activate_cue = true;

    if (coordinator.IsRoleFocused(NavigationItemRole::kTopicSummaryPageScrollContainer)) {
        if (coordinator.scroll_container_active()) {
            // Already scrolling; OK is inert (DOWN double-click exits).
            result.play_activate_cue = false;
            return result;
        }
        result.intent = ActivateIntent::kEnterScroll;
        return result;
    }

    if (coordinator.IsRoleFocused(NavigationItemRole::kTopicSummaryPageBackButton)) {
        result.intent = ActivateIntent::kShowBack;
        return result;
    }

    if (coordinator.IsRoleFocused(NavigationItemRole::kTopicSummaryPageGetSummaryButton)) {
        if (!gemini_ready) {
            // Inline hint already tells the user to connect Gemini; the tap is a no-op.
            result.play_activate_cue = false;
            return result;
        }
        result.intent = ActivateIntent::kRequestSummary;
        return result;
    }

    if (coordinator.IsRoleFocused(NavigationItemRole::kFooterHome)) {
        result.intent = ActivateIntent::kShowHome;
        return result;
    }

    result.handled = false;
    result.play_activate_cue = false;
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
        case ActivateIntent::kEnterScroll:
            if (callbacks.enter_scroll) {
                callbacks.enter_scroll();
            }
            break;
        case ActivateIntent::kRequestSummary:
            if (callbacks.request_summary) {
                callbacks.request_summary();
            }
            break;
        case ActivateIntent::kNone:
        default:
            break;
    }
}

FocusMoveResult HandleMoveFocus(TopicSummaryPageCoordinator& coordinator, int delta)
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

}  // namespace topic_summary_page_interactions
