#ifndef EPAPER_UI_SETTINGS_TOPICS_PAGE_H_
#define EPAPER_UI_SETTINGS_TOPICS_PAGE_H_

#include <cstdint>
#include <string_view>

#include "epaper_ui/button.h"
#include "epaper_ui/global_footer.h"
#include "epaper_ui/select_list.h"
#include "epaper_ui/status_bar.h"

namespace epaper_ui {

struct SettingsTopicsPageState {
    int navigation_focus_index = -1;
    std::string_view title_text = "Topics";
    // One row per topic_service::Topic (label_text = topic name). focused/active both mirror
    // whether roving focus is currently on a topic row -- there is no separate "entered" sub-mode
    // here, unlike the day-grouped timeline lists, since each row is its own top-level roving
    // focus item.
    SelectListState topics_list = {};
    ButtonState new_topic_button = {};
    // Returns to the Settings hub -- this page is reached only from there.
    ButtonState back = {};
};

UiRect SettingsTopicsListBounds(int portrait_width,
                                int portrait_height,
                                const SettingsTopicsPageState& state);
UiRect SettingsTopicsNewTopicButtonBounds(int portrait_width,
                                          int portrait_height,
                                          const SettingsTopicsPageState& state);
UiRect SettingsTopicsBackButtonBounds(int portrait_width,
                                      int portrait_height,
                                      const SettingsTopicsPageState& state);
void DrawSettingsTopicsPage(uint8_t* framebuffer,
                            int raw_width,
                            int raw_height,
                            int portrait_width,
                            int portrait_height,
                            const SettingsTopicsPageState& state,
                            const StatusBarState& status_bar_state,
                            const GlobalFooterState& footer_state);

}  // namespace epaper_ui

#endif  // EPAPER_UI_SETTINGS_TOPICS_PAGE_H_
