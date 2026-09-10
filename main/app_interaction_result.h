#ifndef APP_INTERACTION_RESULT_H_
#define APP_INTERACTION_RESULT_H_

#include <cstdint>

namespace app_interaction {

enum class FeedbackCue : uint8_t {
    kNone = 0,
    kClick,
    kModalOpen,
    kError,
    kRecordingStart,
};

struct InputResult {
    bool consumed = false;
    bool play_feedback = false;
    FeedbackCue feedback_cue = FeedbackCue::kNone;
    bool request_shutdown = false;
    bool request_format_sd_card = false;
    // Set by the OTG modal's single "Disable OTG mode" action.
    bool request_exit_usb_mode = false;
    // Set by the Topics delete-confirm modal's "Delete" action.
    bool request_delete_topic = false;
    bool select_modal_submitted = false;
    int select_modal_selected_index = -1;
};

}  // namespace app_interaction

#endif  // APP_INTERACTION_RESULT_H_
