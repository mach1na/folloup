#ifndef EPAPER_UI_SETTINGS_TODOS_PAGE_H_
#define EPAPER_UI_SETTINGS_TODOS_PAGE_H_

#include <cstdint>
#include <string_view>

#include "epaper_ui/button.h"
#include "epaper_ui/global_footer.h"
#include "epaper_ui/select_input.h"
#include "epaper_ui/status_bar.h"

namespace epaper_ui {

enum class SettingsTodosPageItemId : uint8_t {
    kNone = 0,
    kArchiveAfterInput,
    kBack,
};

struct SettingsTodosPageState {
    int navigation_focus_index = -1;
    std::string_view title_text = "Todos";
    // "Archive todos after" picker: opens a fixed-choice SelectModal, same pattern as the Time
    // page's timezone field.
    SelectInputState archive_after_input = {};
    // Returns to the Settings hub -- this page is reached only from there.
    ButtonState back = {};
};

UiRect SettingsTodosPageItemBounds(int portrait_width,
                                   int portrait_height,
                                   const SettingsTodosPageState& state,
                                   SettingsTodosPageItemId item);
bool HitTestSettingsTodosPageItem(int portrait_width,
                                  int portrait_height,
                                  const SettingsTodosPageState& state,
                                  int x,
                                  int y,
                                  SettingsTodosPageItemId* item);
void DrawSettingsTodosPage(uint8_t* framebuffer,
                           int raw_width,
                           int raw_height,
                           int portrait_width,
                           int portrait_height,
                           const SettingsTodosPageState& state,
                           const StatusBarState& status_bar_state,
                           const GlobalFooterState& footer_state);

}  // namespace epaper_ui

#endif  // EPAPER_UI_SETTINGS_TODOS_PAGE_H_
