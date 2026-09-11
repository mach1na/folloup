#ifndef EPAPER_UI_TOPICS_BROWSE_PAGE_H_
#define EPAPER_UI_TOPICS_BROWSE_PAGE_H_

#include <cstdint>
#include <string_view>

#include "epaper_ui/global_footer.h"
#include "epaper_ui/select_list.h"
#include "epaper_ui/status_bar.h"

namespace epaper_ui {

struct TopicsBrowsePageState {
    int navigation_focus_index = -1;
    std::string_view title_text = "Topics";
    // One row per topic_service::Topic (label_text = topic name). Picking one opens a
    // day-grouped timeline of every entry carrying that topic. focused/active both mirror
    // whether roving focus is currently on a topic row -- there is no separate "entered"
    // sub-mode, matching Settings > Topics' list.
    SelectListState topics_list = {};

    bool operator==(const TopicsBrowsePageState& other) const = default;
};

UiRect TopicsBrowseListBounds(int portrait_width,
                              int portrait_height,
                              const TopicsBrowsePageState& state);
void DrawTopicsBrowsePage(uint8_t* framebuffer,
                          int raw_width,
                          int raw_height,
                          int portrait_width,
                          int portrait_height,
                          const TopicsBrowsePageState& state,
                          const StatusBarState& status_bar_state,
                          const GlobalFooterState& footer_state);

}  // namespace epaper_ui

#endif  // EPAPER_UI_TOPICS_BROWSE_PAGE_H_
