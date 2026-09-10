#ifndef SETTINGS_PAGE_RUNTIME_H_
#define SETTINGS_PAGE_RUNTIME_H_

#include "app_interaction_target.h"
#include "display_service.h"
#include "esp_err.h"
#include "footer_runtime.h"
#include "page_action_result.h"
#include "settings_page_interactions.h"

namespace settings_page_runtime {

esp_err_t UpdateDisplayState();
esp_err_t UpdateDisplayStateAndRequestRefresh(
    display_service::RefreshMode refresh_mode = display_service::RefreshMode::kPartial);
esp_err_t UpdateDisplayStateAndRequestRefresh(
    const display_service::RefreshRequest& refresh_request);
page_actions::FocusMoveOutcome MoveFocus(int delta);
settings_page_interactions::ActivateResult ActivateFocusedItem();

footer_runtime::ProjectionState BuildFooterProjectionState();
page_actions::FocusUpdateOutcome FocusFooterItem(footer_runtime::FooterFocusItem item);
void ResetFocus();

// Deferred so the screen change happens after input dispatch, not mid-dispatch -- app_shell
// polls these, same pattern as onboarding_page_runtime::RequestManualLaunch().
void RequestShowStorage();
bool ConsumePendingShowStorage();
void RequestShowTodos();
bool ConsumePendingShowTodos();
void RequestShowTopics();
bool ConsumePendingShowTopics();

}  // namespace settings_page_runtime

#endif  // SETTINGS_PAGE_RUNTIME_H_
