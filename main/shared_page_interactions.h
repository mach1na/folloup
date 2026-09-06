#ifndef SHARED_PAGE_INTERACTIONS_H_
#define SHARED_PAGE_INTERACTIONS_H_

#include <algorithm>

#include "page_action_result.h"
#include "page_navigation/navigation_model.h"

namespace shared_page_interactions {

struct ScrollStepResult {
    int value = 0;
    bool changed = false;
};

// Steps a [0,100] scroll percentage by `step_percent` in the direction of `delta`, clamped to
// the range. `changed` is false when already at an edge (the step would clamp back to the
// same value) -- callers use this to report "not handled" rather than a no-op state change.
inline ScrollStepResult StepScrollPercent(int current, int delta, int step_percent)
{
    const int step = delta > 0 ? step_percent : -step_percent;
    const int next = std::clamp(current + step, 0, 100);
    return {next, next != current};
}

template <typename ActivateResult, typename ActivateIntent, typename Coordinator>
ActivateResult HandleFooterPrimaryActivate(const Coordinator& coordinator,
                                          ActivateIntent show_home_intent,
                                          ActivateIntent show_settings_intent,
                                          ActivateIntent footer_wifi_intent,
                                          ActivateIntent footer_time_intent)
{
    if (coordinator.IsRoleFocused(page_navigation::NavigationItemRole::kFooterHome)) {
        return {
            .intent = show_home_intent,
            .handled = true,
            .play_activate_cue = true,
        };
    }
    if (coordinator.IsRoleFocused(page_navigation::NavigationItemRole::kFooterSettings)) {
        return {
            .intent = show_settings_intent,
            .handled = true,
            .play_activate_cue = true,
        };
    }
    if (coordinator.IsRoleFocused(page_navigation::NavigationItemRole::kFooterWifi)) {
        return {
            .intent = footer_wifi_intent,
            .handled = true,
            .play_activate_cue = true,
        };
    }
    if (coordinator.IsRoleFocused(page_navigation::NavigationItemRole::kFooterTime)) {
        return {
            .intent = footer_time_intent,
            .handled = true,
            .play_activate_cue = true,
        };
    }

    return {};
}

template <typename Coordinator>
page_actions::FocusMoveOutcome HandleMoveFocus(Coordinator& coordinator, int delta)
{
    if (delta == 0 || !coordinator.MoveFocus(delta)) {
        return {};
    }

    page_actions::FocusMoveOutcome outcome = {};
    outcome.handled = true;
    outcome.play_navigation_cue = true;
    outcome.apply_page_state = true;
    return outcome;
}

}  // namespace shared_page_interactions

#endif  // SHARED_PAGE_INTERACTIONS_H_
