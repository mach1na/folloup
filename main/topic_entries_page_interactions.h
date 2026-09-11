#ifndef TOPIC_ENTRIES_PAGE_INTERACTIONS_H_
#define TOPIC_ENTRIES_PAGE_INTERACTIONS_H_

#include <cstdint>
#include <functional>

#include "page_action_result.h"
#include "topic_entries_page_coordinator.h"

namespace topic_entries_page_interactions {

enum class ActivateIntent : uint8_t {
    kNone = 0,
    kShowHome,
    // Returns to the Topics browse screen -- this page's only possible source.
    kShowBack,
    // Opens the Details page for the selected entry -- this is a browse-only screen, so a click
    // goes straight to Details rather than opening an item-actions modal like Notes/Todos do.
    kViewDetails,
};

struct ActivateResult {
    ActivateIntent intent = ActivateIntent::kNone;
    bool handled = false;
    bool play_activate_cue = false;
    bool apply_page_state = false;
};

using FocusMoveResult = page_actions::FocusMoveOutcome;

struct ActivateCallbacks {
    std::function<void()> show_home;
    std::function<void()> show_back;
    std::function<void()> view_details;
};

// Primary on a focused date chip enters its item list; on a focused item it views the entry's
// details; on the footer it routes to a page.
ActivateResult HandlePrimaryActivate(TopicEntriesPageCoordinator& coordinator);
void ApplyPrimaryActivateResult(const ActivateResult& result, const ActivateCallbacks& callbacks);
FocusMoveResult HandleMoveFocus(TopicEntriesPageCoordinator& coordinator, int delta);

}  // namespace topic_entries_page_interactions

#endif  // TOPIC_ENTRIES_PAGE_INTERACTIONS_H_
