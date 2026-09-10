#ifndef EPAPER_UI_SETTINGS_STORAGE_PAGE_H_
#define EPAPER_UI_SETTINGS_STORAGE_PAGE_H_

#include <cstdint>
#include <string_view>

#include "epaper_ui/button.h"
#include "epaper_ui/global_footer.h"
#include "epaper_ui/sd_status.h"
#include "epaper_ui/status_bar.h"

namespace epaper_ui {

enum class SettingsStoragePageItemId : uint8_t {
    kNone = 0,
    kEnableOtgButton,
    kFormatSdButton,
    kBack,
};

struct SettingsStoragePageState {
    int navigation_focus_index = -1;
    std::string_view title_text = "Storage";
    SdStatusState storage_status = {};
    ButtonState enable_otg_button = {};
    ButtonState format_sd_button = {};
    // Returns to the Settings hub -- this page is reached only from there.
    ButtonState back = {};
};

UiRect SettingsStoragePageItemBounds(int portrait_width,
                                     int portrait_height,
                                     const SettingsStoragePageState& state,
                                     SettingsStoragePageItemId item);
bool HitTestSettingsStoragePageItem(int portrait_width,
                                    int portrait_height,
                                    const SettingsStoragePageState& state,
                                    int x,
                                    int y,
                                    SettingsStoragePageItemId* item);
void DrawSettingsStoragePage(uint8_t* framebuffer,
                             int raw_width,
                             int raw_height,
                             int portrait_width,
                             int portrait_height,
                             const SettingsStoragePageState& state,
                             const StatusBarState& status_bar_state,
                             const GlobalFooterState& footer_state);

}  // namespace epaper_ui

#endif  // EPAPER_UI_SETTINGS_STORAGE_PAGE_H_
