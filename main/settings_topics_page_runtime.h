#ifndef SETTINGS_TOPICS_PAGE_RUNTIME_H_
#define SETTINGS_TOPICS_PAGE_RUNTIME_H_

#include "app_interaction_target.h"
#include "display_service.h"
#include "esp_err.h"
#include "footer_runtime.h"
#include "page_action_result.h"
#include "settings_topics_page_interactions.h"

namespace settings_topics_page_runtime {

esp_err_t UpdateDisplayState();
esp_err_t UpdateDisplayStateAndRequestRefresh(
    display_service::RefreshMode refresh_mode = display_service::RefreshMode::kPartial);
esp_err_t UpdateDisplayStateAndRequestRefresh(
    const display_service::RefreshRequest& refresh_request);
page_actions::FocusMoveOutcome MoveFocus(int delta);
settings_topics_page_interactions::ActivateResult ActivateFocusedItem();

footer_runtime::ProjectionState BuildFooterProjectionState();
page_actions::FocusUpdateOutcome FocusFooterItem(footer_runtime::FooterFocusItem item);
void ResetFocus();
// Re-syncs the topic list from topic_service (after a create/rename/delete, or from the
// topic_service event handler) and repaints if this page is the one currently on screen.
esp_err_t SyncFromTopicService(bool request_refresh_if_active);

// Opens the Rename/Delete action modal for the currently-focused topic row. False if focus
// isn't on a row.
bool ShowItemActionsModal();
// Routed from app_shell's select-modal-submit dispatch chain. Returns false (not consumed)
// unless this page's own action modal was the one open.
bool HandleItemActionSelection(int selected_index);

// Called when "New Topic" is activated -- opens the keyboard, empty. Voice-created topics are
// created via the normal recording flow's "Save recording as" menu instead (see the "Topic"
// option added to recording_session_service::TagOptions), not from this screen.
void HandleNewTopicActivated();
// Called once the Topics delete-confirm card modal's "Delete" action fires. Returns false if
// there was no pending delete.
bool DeleteConfirmedTopic();

}  // namespace settings_topics_page_runtime

#endif  // SETTINGS_TOPICS_PAGE_RUNTIME_H_
