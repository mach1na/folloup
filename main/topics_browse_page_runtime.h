#ifndef TOPICS_BROWSE_PAGE_RUNTIME_H_
#define TOPICS_BROWSE_PAGE_RUNTIME_H_

#include <string>

#include "app_interaction_target.h"
#include "display_service.h"
#include "esp_err.h"
#include "footer_runtime.h"
#include "page_action_result.h"
#include "topics_browse_page_interactions.h"

namespace topics_browse_page_runtime {

esp_err_t UpdateDisplayState();
esp_err_t UpdateDisplayStateAndRequestRefresh(
    display_service::RefreshMode refresh_mode = display_service::RefreshMode::kPartial);
esp_err_t UpdateDisplayStateAndRequestRefresh(
    const display_service::RefreshRequest& refresh_request);
page_actions::FocusMoveOutcome MoveFocus(int delta);
topics_browse_page_interactions::ActivateResult ActivateFocusedItem();

footer_runtime::ProjectionState BuildFooterProjectionState();
page_actions::FocusUpdateOutcome FocusFooterItem(footer_runtime::FooterFocusItem item);
void ResetFocus();
// Re-syncs the topic list from topic_service (after a create/rename/delete elsewhere, or from
// the topic_service event handler) and repaints if this page is the one currently on screen.
esp_err_t SyncFromTopicService(bool request_refresh_if_active);

// Called when a topic row is activated. Stashes the topic id/name for the deferred screen
// transition (app_shell polls ConsumePendingShowEntries after input dispatch returns).
void RequestShowEntriesForFocusedTopic();
struct PendingTopicEntries {
    bool valid = false;
    std::string topic_id = {};
    std::string topic_name = {};
};
PendingTopicEntries ConsumePendingShowEntries();

}  // namespace topics_browse_page_runtime

#endif  // TOPICS_BROWSE_PAGE_RUNTIME_H_
