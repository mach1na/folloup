#ifndef TOPIC_SUMMARY_PAGE_INTERACTIONS_H_
#define TOPIC_SUMMARY_PAGE_INTERACTIONS_H_

#include <cstdint>
#include <functional>

#include "page_action_result.h"
#include "topic_summary_page_coordinator.h"

namespace topic_summary_page_interactions {

enum class ActivateIntent : uint8_t {
    kNone = 0,
    kShowHome,
    // Returns to the Topic Entries screen for this topic -- this page's only possible source.
    kShowBack,
    kEnterScroll,
    kRequestSummary,
};

struct ActivateResult {
    ActivateIntent intent = ActivateIntent::kNone;
    bool handled = false;
    bool play_activate_cue = false;
};

using FocusMoveResult = page_actions::FocusMoveOutcome;

struct ActivateCallbacks {
    std::function<void()> show_home;
    std::function<void()> show_back;
    std::function<void()> enter_scroll;
    std::function<void()> request_summary;
};

// gemini_ready gates the Get/Refresh-summary action: when Gemini isn't connected the tap is a
// no-op (the scroll container already shows the "Connect to Gemini" hint).
ActivateResult HandlePrimaryActivate(TopicSummaryPageCoordinator& coordinator, bool gemini_ready);
void ApplyPrimaryActivateResult(const ActivateResult& result, const ActivateCallbacks& callbacks);
FocusMoveResult HandleMoveFocus(TopicSummaryPageCoordinator& coordinator, int delta);

}  // namespace topic_summary_page_interactions

#endif  // TOPIC_SUMMARY_PAGE_INTERACTIONS_H_
