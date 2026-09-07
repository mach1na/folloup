#ifndef DEVICE_SLEEP_RUNTIME_H_
#define DEVICE_SLEEP_RUNTIME_H_

#include <cstdint>

#include "button_service.h"
#include "esp_err.h"

namespace device_sleep_runtime {

using ShutdownPendingProvider = bool (*)(void* context);

struct AutoSleepSettings {
    bool enabled = true;
    uint32_t display_sleep_timeout_seconds = 30;
    uint32_t light_sleep_timeout_seconds = 90;
    bool motion_wake_enabled = true;
    bool interaction_wake_enabled = true;
};

void SetShutdownPendingProvider(ShutdownPendingProvider provider, void* context);
esp_err_t StartAutoSleep(const AutoSleepSettings& settings);
esp_err_t StartMotionPolling();
void NotifyUserActivity();

// Marks the POWER_OK press that is waking the device so it only wakes.
//
// Every event that press produces is swallowed by ConsumeWakeOnlyPowerButtonEvent
// until the gesture resolves, with one deliberate exception: DOUBLE_CLICK is let
// through, so waking and toggling the lock screen stays a single gesture. Armed by
// the light-sleep exit path and by the display-sleep wake in app_shell.
void ArmPowerButtonWakeGesture(const char* reason);
bool ConsumeWakeOnlyPowerButtonEvent(const button_service::ButtonEventInfo& event);

// Marks the display/light-sleep wake this triggers as also authorized to unlock
// straight to the restore screen (e.g. Home) in the same refresh, instead of the
// default of just waking the display back up on whatever it already showed (the lock
// screen). Call immediately before the NotifyUserActivity()/wake that this same press
// causes. Scoped to the specific press that requested it (consumed by the next wake
// action dispatch) so that only the deliberate lock/unlock gesture -- not some other
// wake source, such as the ACTION button also being a light-sleep wake source -- skips
// straight past the lock screen.
void RequestUnlockOnWake();

}  // namespace device_sleep_runtime

#endif  // DEVICE_SLEEP_RUNTIME_H_
