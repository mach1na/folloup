#ifndef SETTINGS_TODOS_PAGE_INTERACTIONS_H_
#define SETTINGS_TODOS_PAGE_INTERACTIONS_H_

#include <cstdint>
#include <functional>

#include "page_action_result.h"
#include "settings_todos_page_coordinator.h"

namespace settings_todos_page_interactions {

enum class ActivateIntent : uint8_t {
    kNone = 0,
    kShowHome,
    kShowSettings,
    kShowArchiveAfterModal,
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
    std::function<void()> show_archive_after_modal;
};

ActivateResult HandlePrimaryActivate(const SettingsTodosPageCoordinator& coordinator);
void ApplyPrimaryActivateResult(const ActivateResult& result, const ActivateCallbacks& callbacks);
FocusMoveResult HandleMoveFocus(SettingsTodosPageCoordinator& coordinator, int delta);

}  // namespace settings_todos_page_interactions

#endif  // SETTINGS_TODOS_PAGE_INTERACTIONS_H_
