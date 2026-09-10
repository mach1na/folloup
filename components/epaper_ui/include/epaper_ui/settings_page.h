#ifndef EPAPER_UI_SETTINGS_PAGE_H_
#define EPAPER_UI_SETTINGS_PAGE_H_

#include <cstdint>
#include <string_view>

#include "epaper_ui/button.h"
#include "epaper_ui/global_footer.h"
#include "epaper_ui/menu_container.h"
#include "epaper_ui/menu_item.h"
#include "epaper_ui/status_bar.h"

namespace epaper_ui {

// Settings is a hub: 4 headings, each navigating to a dedicated sub-page, in this fixed slot
// order (must match kSettingsMenuLabels in settings_page.cpp).
inline constexpr int kSettingsMenuItemCount = 4;

enum class SettingsMenuItem : int {
    kNetwork = 0,
    kTime,
    kStorage,
    kTodos,
};

struct SettingsPageMenuState {
    int selected_index = -1;

    bool operator==(const SettingsPageMenuState& other) const = default;
};

struct SettingsPageState {
    int navigation_focus_index = -1;
    std::string_view title_text = "Settings";
    SettingsPageMenuState menu = {};
    // Replays the onboarding carousel -- a one-shot action, not a heading, so it sits below the
    // menu rather than being one of the 4 headings above.
    ButtonState manual_button = {};

    bool operator==(const SettingsPageState& other) const = default;
};

const char* SettingsMenuItemLabel(int index);
UiRect SettingsMenuItemBounds(int portrait_width,
                              int portrait_height,
                              const SettingsPageState& state,
                              int index);
bool HitTestSettingsMenuItem(int portrait_width,
                             int portrait_height,
                             const SettingsPageState& state,
                             int x,
                             int y,
                             int* index);
UiRect SettingsManualButtonBounds(int portrait_width,
                                  int portrait_height,
                                  const SettingsPageState& state);
bool HitTestSettingsManualButton(int portrait_width,
                                 int portrait_height,
                                 const SettingsPageState& state,
                                 int x,
                                 int y);
void DrawSettingsPage(uint8_t* framebuffer,
                      int raw_width,
                      int raw_height,
                      int portrait_width,
                      int portrait_height,
                      const SettingsPageState& state,
                      const StatusBarState& status_bar_state,
                      const GlobalFooterState& footer_state);

}  // namespace epaper_ui

#endif  // EPAPER_UI_SETTINGS_PAGE_H_
