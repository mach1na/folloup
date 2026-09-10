#ifndef SETTINGS_TOPICS_PAGE_INTERACTIONS_H_
#define SETTINGS_TOPICS_PAGE_INTERACTIONS_H_

#include <cstdint>
#include <functional>

#include "page_action_result.h"
#include "settings_topics_page_coordinator.h"

namespace settings_topics_page_interactions {

enum class ActivateIntent : uint8_t {
    kNone = 0,
    kShowHome,
    kShowSettings,
    // Opens the Rename/Delete action modal for the focused topic row.
    kOpenTopicActions,
    kNewTopic,
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
    std::function<void()> open_topic_actions;
    std::function<void()> new_topic;
};

ActivateResult HandlePrimaryActivate(const SettingsTopicsPageCoordinator& coordinator);
void ApplyPrimaryActivateResult(const ActivateResult& result, const ActivateCallbacks& callbacks);
FocusMoveResult HandleMoveFocus(SettingsTopicsPageCoordinator& coordinator, int delta);

}  // namespace settings_topics_page_interactions

#endif  // SETTINGS_TOPICS_PAGE_INTERACTIONS_H_
