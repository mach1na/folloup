#ifndef TOPICS_BROWSE_PAGE_INTERACTIONS_H_
#define TOPICS_BROWSE_PAGE_INTERACTIONS_H_

#include <cstdint>
#include <functional>

#include "page_action_result.h"
#include "topics_browse_page_coordinator.h"

namespace topics_browse_page_interactions {

enum class ActivateIntent : uint8_t {
    kNone = 0,
    kShowHome,
    // Opens a filtered timeline for the focused topic.
    kSelectTopic,
};

struct ActivateResult {
    ActivateIntent intent = ActivateIntent::kNone;
    bool handled = false;
    bool play_activate_cue = false;
};

using FocusMoveResult = page_actions::FocusMoveOutcome;

struct ActivateCallbacks {
    std::function<void()> show_home;
    std::function<void()> select_topic;
};

ActivateResult HandlePrimaryActivate(const TopicsBrowsePageCoordinator& coordinator);
void ApplyPrimaryActivateResult(const ActivateResult& result, const ActivateCallbacks& callbacks);
FocusMoveResult HandleMoveFocus(TopicsBrowsePageCoordinator& coordinator, int delta);

}  // namespace topics_browse_page_interactions

#endif  // TOPICS_BROWSE_PAGE_INTERACTIONS_H_
