#ifndef SETTINGS_TOPICS_PAGE_RUNTIME_H_
#define SETTINGS_TOPICS_PAGE_RUNTIME_H_

#include "app_interaction_target.h"
#include "button_service.h"
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

// Called when "New Topic" is activated by a quick tap (kSingleClick) -- always opens the
// keyboard, empty. Voice creation is a press-and-hold gesture instead; see
// HandleActionButtonEvent below.
void HandleNewTopicActivated();
// Called from app_shell's global ACTION-button handling, before it would otherwise fall through
// to recording_session_service's app-wide press-and-hold-to-record gesture. Returns false
// (event not consumed, let the normal recording gesture proceed) unless "New Topic" is the
// currently-focused item on this page -- in which case the hold is redirected to a topic voice
// capture: press-and-hold records, release stops it, transcribes, and opens the keyboard
// pre-filled with the extracted name. Falls back to a toast + empty keyboard if Gemini isn't
// ready (no Wi-Fi / not authenticated) when the hold begins.
bool HandleActionButtonEvent(const button_service::ButtonEventInfo& event);
// Called once the Topics delete-confirm card modal's "Delete" action fires. Returns false if
// there was no pending delete.
bool DeleteConfirmedTopic();

}  // namespace settings_topics_page_runtime

#endif  // SETTINGS_TOPICS_PAGE_RUNTIME_H_
