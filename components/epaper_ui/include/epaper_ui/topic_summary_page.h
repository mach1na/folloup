#ifndef EPAPER_UI_TOPIC_SUMMARY_PAGE_H_
#define EPAPER_UI_TOPIC_SUMMARY_PAGE_H_

#include <cstdint>
#include <string>

#include "epaper_ui/button.h"
#include "epaper_ui/global_footer.h"
#include "epaper_ui/overlay_geometry.h"
#include "epaper_ui/scroll_container.h"
#include "epaper_ui/status_bar.h"

namespace epaper_ui {

struct TopicSummaryPageState {
    int navigation_focus_index = -1;
    // The topic's name -- set per-topic, same as Topic Entries' title.
    std::string title_text = "Topic";
    std::string subtitle_text = "Summary";
    ScrollContainerState scroll_container = {};
    // Back (secondary) returns to Topic Entries; the primary action gets/refreshes the summary --
    // its label switches between "Get summary" and "Refresh summary" based on whether one already
    // exists, same shape as Details' Play/Transcribe button.
    ButtonState back_button = {};
    ButtonState get_summary_button = {};

    bool operator==(const TopicSummaryPageState& other) const = default;
};

void DrawTopicSummaryPage(uint8_t* framebuffer,
                          int raw_width,
                          int raw_height,
                          int portrait_width,
                          int portrait_height,
                          const TopicSummaryPageState& state,
                          const StatusBarState& status_bar_state,
                          const GlobalFooterState& footer_state);

}  // namespace epaper_ui

#endif  // EPAPER_UI_TOPIC_SUMMARY_PAGE_H_
