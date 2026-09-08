#ifndef SHARED_PAGE_INTERACTIONS_H_
#define SHARED_PAGE_INTERACTIONS_H_

#include <algorithm>

#include "footer_runtime.h"
#include "page_action_result.h"
#include "page_navigation/navigation_model.h"
#include "page_navigation/page_focus_projection.h"

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

// Footer index<->role mapping. This exact mapping (0=Home, 1=Settings, 2=Wifi, 3=Time, 4=Sticky) is
// duplicated identically across every page runtime in the app; scoped here to the Notes/Todos/
// Follow-up trio for now rather than migrating every other page's already-working copy in the same
// change (a separate, lower-risk cleanup).
inline footer_runtime::FooterFocusItem FooterItemForIndex(int selected_index)
{
    switch (selected_index) {
        case 1:
            return footer_runtime::FooterFocusItem::kSettings;
        case 2:
            return footer_runtime::FooterFocusItem::kWifi;
        case 3:
            return footer_runtime::FooterFocusItem::kTime;
        case 0:
            return footer_runtime::FooterFocusItem::kHome;
        case 4:
            return footer_runtime::FooterFocusItem::kSticky;
        default:
            return footer_runtime::FooterFocusItem::kNone;
    }
}

inline page_navigation::NavigationItemRole FooterRoleForFooterItem(
    footer_runtime::FooterFocusItem item)
{
    switch (item) {
        case footer_runtime::FooterFocusItem::kSettings:
            return page_navigation::NavigationItemRole::kFooterSettings;
        case footer_runtime::FooterFocusItem::kWifi:
            return page_navigation::NavigationItemRole::kFooterWifi;
        case footer_runtime::FooterFocusItem::kHome:
            return page_navigation::NavigationItemRole::kFooterHome;
        case footer_runtime::FooterFocusItem::kTime:
            return page_navigation::NavigationItemRole::kFooterTime;
        case footer_runtime::FooterFocusItem::kSticky:
            return page_navigation::NavigationItemRole::kFooterSticky;
        case footer_runtime::FooterFocusItem::kNone:
        case footer_runtime::FooterFocusItem::kFolder:
        case footer_runtime::FooterFocusItem::kMic:
        default:
            return page_navigation::NavigationItemRole::kUnknown;
    }
}

// Projects a two-level timeline page's current focus into a footer ProjectionState (which footer
// icon, if any, should read as highlighted). `group_section` is the page's own timeline-group
// section (e.g. kTodosPageTimelineGroups), needed only to build the PageFocusProjection query --
// this function doesn't otherwise care about groups/items, just the resulting footer_selected_index.
template <typename Coordinator>
footer_runtime::ProjectionState BuildTimelineFooterProjectionState(
    const Coordinator& coordinator, page_navigation::NavigationItemSection group_section)
{
    const page_navigation::PageFocusProjection projection = page_navigation::ProjectPageFocus(
        coordinator.navigation_model(), group_section, coordinator.focus().index(), -1, -1);
    footer_runtime::ProjectionState state = {};
    state.focused_item = FooterItemForIndex(projection.footer_selected_index);
    return state;
}

// True when moving focus from old_focus_index to new_focus_index would change which footer icon
// (if any) reads as highlighted -- used to decide whether the footer widget needs a repaint after
// a focus move.
template <typename Coordinator>
bool TimelineFooterProjectionChanged(const Coordinator& coordinator, int old_focus_index,
                                     int new_focus_index,
                                     page_navigation::NavigationItemSection group_section)
{
    const page_navigation::PageFocusProjection old_projection = page_navigation::ProjectPageFocus(
        coordinator.navigation_model(), group_section, old_focus_index, -1, -1);
    const page_navigation::PageFocusProjection new_projection = page_navigation::ProjectPageFocus(
        coordinator.navigation_model(), group_section, new_focus_index, -1, -1);
    return FooterItemForIndex(old_projection.footer_selected_index) !=
           FooterItemForIndex(new_projection.footer_selected_index);
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
