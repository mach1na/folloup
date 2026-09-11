#ifndef TOPIC_ENTRIES_PAGE_RUNTIME_H_
#define TOPIC_ENTRIES_PAGE_RUNTIME_H_

#include <string>

#include "app_interaction_target.h"
#include "display_service.h"
#include "esp_err.h"
#include "footer_runtime.h"
#include "page_action_result.h"
#include "topic_entries_page_interactions.h"

namespace topic_entries_page_runtime {

esp_err_t UpdateDisplayState();
esp_err_t UpdateDisplayStateAndRequestRefresh(
    display_service::RefreshMode refresh_mode = display_service::RefreshMode::kPartial);
esp_err_t UpdateDisplayStateAndRequestRefresh(
    const display_service::RefreshRequest& refresh_request);

page_actions::FocusMoveOutcome MoveFocus(int delta);
topic_entries_page_interactions::ActivateResult ActivateFocusedItem();

footer_runtime::ProjectionState BuildFooterProjectionState();
page_actions::FocusUpdateOutcome FocusFooterItem(footer_runtime::FooterFocusItem item);
void ResetFocus();

// Stash the topic to show, then read the archive (SD) and (re)build the page.
void QueueShow(const std::string& topic_id, const std::string& topic_name);
esp_err_t SyncFromArchive(bool request_refresh_if_active);

// Leave an entered item list (DOWN double-click). Returns true if a list was active.
bool ExitActiveControl();

// Back navigation is deferred so it happens after input dispatch returns (avoids changing
// screens mid-dispatch, same pattern Details' RequestBack uses). App_shell polls
// ConsumePendingBack() and returns to the Topics browse screen.
void RequestBack();
bool ConsumePendingBack();

// Stashes the currently-selected entry's recording id for the deferred Details transition
// (called from the kViewDetails activation callback).
void RequestViewDetails();
// View details is deferred the same way as Back. Empty when none is pending.
std::string ConsumePendingViewDetails();

// Stashes this topic's id/name for the deferred Topic Summary transition (called from the
// kSummarize activation callback). App_shell polls ConsumePendingShowSummary() after input
// dispatch returns, same pattern as topics_browse_page_runtime's RequestShowEntriesForFocusedTopic.
void RequestShowSummary();
struct PendingTopicSummary {
    bool valid = false;
    std::string topic_id = {};
    std::string topic_name = {};
};
PendingTopicSummary ConsumePendingShowSummary();

}  // namespace topic_entries_page_runtime

#endif  // TOPIC_ENTRIES_PAGE_RUNTIME_H_
