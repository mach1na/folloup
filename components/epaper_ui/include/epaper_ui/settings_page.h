#ifndef EPAPER_UI_SETTINGS_PAGE_H_
#define EPAPER_UI_SETTINGS_PAGE_H_

#include <cstdint>
#include <string_view>

#include "epaper_ui/button.h"
#include "epaper_ui/global_footer.h"
#include "epaper_ui/menu_toggle.h"
#include "epaper_ui/sd_status.h"
#include "epaper_ui/select_input.h"
#include "epaper_ui/status_bar.h"

namespace epaper_ui {

enum class SettingsPageItemId : uint8_t {
    kNone = 0,
    kWifiToggle,
    kAccessPointToggle,
    kEnableOtgButton,
    kFormatSdButton,
    kManualOnboardingButton,
    kArchiveAfterInput,
};

struct SettingsPageState {
    int navigation_focus_index = -1;
    std::string_view title_text = "Settings";
    MenuToggleState wifi_toggle = {};
    MenuToggleState access_point_toggle = {};
    SdStatusState storage_status = {};
    ButtonState enable_otg_button = {};
    ButtonState format_sd_button = {};
    ButtonState manual_onboarding_button = {};
    // "Archive todos after" picker: opens a fixed-choice SelectModal, same pattern as the Time
    // page's timezone field.
    SelectInputState archive_after_input = {};
    // Which SettingsPageItemId (by its 0-based order: wifi_toggle=0 .. archive_after_input=5)
    // should anchor the top of the scrollable content area. Set by the caller (main/) as a hint,
    // updated only when focus moves to a new item -- see SettingsPageResolveVisibleAnchor, which
    // adjusts it the minimum amount needed to keep the focused item on screen, mirroring
    // TimelineListState::visible_group_index's role for the Notes/Todos/Follow-up timelines.
    int visible_item_index = 0;
};

// Given the coordinator's current visible_item_index hint (state.visible_item_index) and which
// item is currently focused (derived from each item's own focused/selected field), returns the
// anchor index that should be stored back for the next render: unchanged if the focused item is
// already fully visible with the current anchor, otherwise adjusted the minimum amount (one item
// at a time) so it becomes visible. Callers (main/settings_page_runtime.cpp) call this once after
// any focus move and feed the result back into the coordinator; BuildLayout/DrawSettingsPage then
// just trust state.visible_item_index as already-resolved on every render, rather than
// re-searching every time.
int SettingsPageResolveVisibleAnchor(int portrait_width,
                                     int portrait_height,
                                     const SettingsPageState& state);

UiRect SettingsPageItemBounds(int portrait_width,
                              int portrait_height,
                              const SettingsPageState& state,
                              SettingsPageItemId item);
UiRect SettingsPageItemVisualBounds(int portrait_width,
                                    int portrait_height,
                                    const SettingsPageState& state,
                                    SettingsPageItemId item);
bool HitTestSettingsPageItem(int portrait_width,
                             int portrait_height,
                             const SettingsPageState& state,
                             int x,
                             int y,
                             SettingsPageItemId* item);
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
