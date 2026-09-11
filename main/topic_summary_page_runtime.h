#ifndef TOPIC_SUMMARY_PAGE_RUNTIME_H_
#define TOPIC_SUMMARY_PAGE_RUNTIME_H_

#include <string>

#include "app_interaction_target.h"
#include "display_service.h"
#include "esp_err.h"
#include "footer_runtime.h"
#include "page_action_result.h"
#include "summary_service.h"
#include "topic_summary_page_interactions.h"

namespace topic_summary_page_runtime {

esp_err_t UpdateDisplayState();
esp_err_t UpdateDisplayStateAndRequestRefresh(
    display_service::RefreshMode refresh_mode = display_service::RefreshMode::kPartial);
esp_err_t UpdateDisplayStateAndRequestRefresh(
    const display_service::RefreshRequest& refresh_request);

page_actions::FocusMoveOutcome MoveFocus(int delta);
topic_summary_page_interactions::ActivateResult ActivateFocusedItem();

footer_runtime::ProjectionState BuildFooterProjectionState();
page_actions::FocusUpdateOutcome FocusFooterItem(footer_runtime::FooterFocusItem item);
void ResetFocus();

// Stash the topic to show, then load its cached summary (SD) and (re)build the page.
void QueueShow(const std::string& topic_id, const std::string& topic_name);
esp_err_t SyncFromService(bool request_refresh_if_active);
// Adopt a summary snapshot delivered by an event for the topic currently showing (app_shell has
// already filtered to kind == kTopic && matching topic_id) and refresh if the page is active.
esp_err_t OnSummarySnapshot(const summary_service::Snapshot& snapshot, bool request_refresh);

// Activation effects invoked from the page-input callbacks.
void EnterScroll();
void RequestSummary();
// Leave an entered scroll container (DOWN double-click). Returns true if it was active.
bool ExitActiveControl();

// Back navigation is deferred so it happens after input dispatch returns (same pattern as Topic
// Entries' RequestBack). App_shell polls ConsumePendingBack() and returns to Topic Entries.
void RequestBack();
bool ConsumePendingBack();
std::string CurrentTopicId();

}  // namespace topic_summary_page_runtime

#endif  // TOPIC_SUMMARY_PAGE_RUNTIME_H_
