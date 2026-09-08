#ifndef LOCK_SCREEN_RUNTIME_H_
#define LOCK_SCREEN_RUNTIME_H_

#include "display_service.h"
#include "esp_err.h"

namespace lock_screen_runtime {

esp_err_t Init();
bool IsActive();
esp_err_t Show();
// Like Show(), but for freezing the panel on the lock screen's todo summary as the
// last thing painted before a full power-off. Paints synchronously via
// display_service::WakeDisplayToScreen (blocks until the panel hardware is actually
// done, and works correctly even if the panel is currently asleep -- unlike Show()'s
// SetCurrentScreen path, which is silently dropped in that case). Skips Show()'s
// trailing ForceDisplaySleep() call, which would be redundant/wrong immediately
// before a hard power cut.
esp_err_t ShowForShutdown();
esp_err_t Hide();
// Like Hide(), but for waking directly out of display/light sleep while locked: wakes
// the panel straight to the restore screen in one full refresh instead of redrawing the
// lock screen first. Callers must already know the panel is currently asleep.
esp_err_t HideWaking();
esp_err_t Toggle();
esp_err_t RequestRefresh(
    display_service::RefreshMode refresh_mode = display_service::RefreshMode::kPartial);
esp_err_t SyncClockState(bool request_refresh_if_active);
// Kicks off (on its own dedicated task, never the caller's) a re-scan of the recording
// archive and recomputes the pending-todo summary shown on the lock screen: up to three
// pending todos (follow-up flagged ones first, then newest first), plus the true total
// pending count. Non-blocking -- safe to call from any task, including one with a small
// stack (an archive-changed event can fire from very different callers). Pushes the result
// into the lock screen's state once the scan completes and, if the lock screen is
// currently active, requests a partial refresh -- so by the time a user actually locks
// the device, the summary is already current rather than being computed at lock time. A
// call while a refresh is already in flight is a no-op (the pending one will see current
// state), not an error.
esp_err_t RefreshTodoSummary();

}  // namespace lock_screen_runtime

#endif  // LOCK_SCREEN_RUNTIME_H_
