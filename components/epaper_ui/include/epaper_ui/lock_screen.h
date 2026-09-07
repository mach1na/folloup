#ifndef EPAPER_UI_LOCK_SCREEN_H_
#define EPAPER_UI_LOCK_SCREEN_H_

#include <string>
#include <vector>

#include "epaper_ui/status_bar.h"

namespace epaper_ui {

struct LockScreenState {
    std::string weekday_text = {};
    std::string date_text = {};
    // The top few pending todos, already selected and prioritized (follow-up flagged ones
    // first) by the caller. Full transcript text, untruncated -- DrawLockScreen wraps and
    // caps each one to fit.
    std::vector<std::string> pending_todo_titles = {};
    // True total count of pending todos, which may be larger than pending_todo_titles.size().
    int pending_todo_count = 0;
};

void DrawLockScreen(uint8_t* framebuffer,
                    int raw_width,
                    int raw_height,
                    int portrait_width,
                    int portrait_height,
                    const LockScreenState& state,
                    const StatusBarState& status_state);

}  // namespace epaper_ui

#endif  // EPAPER_UI_LOCK_SCREEN_H_
