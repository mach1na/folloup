#ifndef LOCK_SCREEN_RUNTIME_H_
#define LOCK_SCREEN_RUNTIME_H_

#include "display_service.h"
#include "esp_err.h"

namespace lock_screen_runtime {

esp_err_t Init();
bool IsActive();
esp_err_t Show();
esp_err_t Hide();
// Like Hide(), but for waking directly out of display/light sleep while locked: wakes
// the panel straight to the restore screen in one full refresh instead of redrawing the
// lock screen first. Callers must already know the panel is currently asleep.
esp_err_t HideWaking();
esp_err_t Toggle();
esp_err_t RequestRefresh(
    display_service::RefreshMode refresh_mode = display_service::RefreshMode::kPartial);
esp_err_t SyncClockState(bool request_refresh_if_active);

}  // namespace lock_screen_runtime

#endif  // LOCK_SCREEN_RUNTIME_H_
