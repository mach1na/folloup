#ifndef SETTINGS_STORAGE_PAGE_INTERACTIONS_H_
#define SETTINGS_STORAGE_PAGE_INTERACTIONS_H_

#include <cstdint>
#include <functional>

#include "page_action_result.h"
#include "settings_storage_page_coordinator.h"

namespace settings_storage_page_interactions {

enum class ActivateIntent : uint8_t {
    kNone = 0,
    kShowHome,
    kShowSettings,
    kEnableOtg,
    kShowFormatSdModal,
};

struct ActivateResult {
    ActivateIntent intent = ActivateIntent::kNone;
    bool handled = false;
    bool play_activate_cue = false;
};

using FocusMoveResult = page_actions::FocusMoveOutcome;

struct ActivateCallbacks {
    std::function<void()> show_home;
    std::function<void()> show_settings;
    std::function<void()> enable_otg;
    std::function<void()> show_format_sd_modal;
};

ActivateResult HandlePrimaryActivate(const SettingsStoragePageCoordinator& coordinator);
void ApplyPrimaryActivateResult(const ActivateResult& result, const ActivateCallbacks& callbacks);
FocusMoveResult HandleMoveFocus(SettingsStoragePageCoordinator& coordinator, int delta);

}  // namespace settings_storage_page_interactions

#endif  // SETTINGS_STORAGE_PAGE_INTERACTIONS_H_
