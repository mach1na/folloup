#ifndef EPAPER_UI_TOPIC_ENTRIES_PAGE_H_
#define EPAPER_UI_TOPIC_ENTRIES_PAGE_H_

#include <cstdint>
#include <string>

#include "epaper_ui/button.h"
#include "epaper_ui/global_footer.h"
#include "epaper_ui/status_bar.h"
#include "epaper_ui/timeline_list.h"

namespace epaper_ui {

struct TopicEntriesPageState {
    int navigation_focus_index = -1;
    // The topic's name (e.g. "Kitchen Renovation") -- set per-topic, unlike Notes/Todos'/
    // Follow-up's fixed title.
    std::string title_text = "Topic";
    TimelineListState timeline = {};
    // Reached only from the Topics browse screen (picking a topic); Back returns there, not Home
    // -- the footer's Home icon still means literal Home, same split Details uses.
    ButtonState back_button = {};
    // Opens the topic's summary screen. Hidden for an empty topic -- nothing to summarize yet --
    // same optional-second-button shape as Details' Transcribe button.
    bool show_summarize_button = false;
    ButtonState summarize_button = {};

    bool operator==(const TopicEntriesPageState& other) const = default;
};

void DrawTopicEntriesPage(uint8_t* framebuffer,
                          int raw_width,
                          int raw_height,
                          int portrait_width,
                          int portrait_height,
                          const TopicEntriesPageState& state,
                          const StatusBarState& status_bar_state,
                          const GlobalFooterState& footer_state);

}  // namespace epaper_ui

#endif  // EPAPER_UI_TOPIC_ENTRIES_PAGE_H_
