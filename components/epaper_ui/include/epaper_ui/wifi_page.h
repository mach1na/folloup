#ifndef EPAPER_UI_WIFI_PAGE_H_
#define EPAPER_UI_WIFI_PAGE_H_

#include <cstdint>
#include <string>

#include "epaper_ui/button.h"
#include "epaper_ui/global_footer.h"
#include "epaper_ui/menu_toggle.h"
#include "epaper_ui/network_list.h"
#include "epaper_ui/password_input.h"
#include "epaper_ui/status_bar.h"

namespace epaper_ui {

enum class WifiPageItemId : uint8_t {
    kNone = 0,
    kWifiToggle,
    kEnableApToggle,
    kNetworkList,
    kPasswordInput,
    kPasswordVisibilityButton,
    kScanButton,
    kConnectButton,
    kBack,
};

struct WifiPageState {
    int navigation_focus_index = -1;
    std::string title_text = "WiFi Setup";
    // Moved here from the old flat Settings page -- this is now the "Network" section of the
    // Settings hub.
    MenuToggleState wifi_toggle = {};
    MenuToggleState access_point_toggle = {};
    NetworkListState network_list = {};
    PasswordInputState password_input = {};
    ButtonState scan_button = {};
    ButtonState connect_button = {};
    // Returns to the Settings hub -- this page is reached only from there now, not a footer icon.
    ButtonState back = {};

    bool operator==(const WifiPageState& other) const = default;
};

UiRect WifiPageItemBounds(int portrait_width,
                          int portrait_height,
                          const WifiPageState& state,
                          WifiPageItemId item);
UiRect WifiPageItemVisualBounds(int portrait_width,
                                int portrait_height,
                                const WifiPageState& state,
                                WifiPageItemId item);
UiRect WifiPageNetworkRowBounds(int portrait_width,
                                int portrait_height,
                                const WifiPageState& state,
                                int row_index);
int WifiPageFirstVisibleNetworkRow(int portrait_width,
                                   int portrait_height,
                                   const WifiPageState& state);
bool HitTestWifiPageItem(int portrait_width,
                         int portrait_height,
                         const WifiPageState& state,
                         int x,
                         int y,
                         WifiPageItemId* item);
bool HitTestWifiPageNetworkRow(int portrait_width,
                               int portrait_height,
                               const WifiPageState& state,
                               int x,
                               int y,
                               int* row_index);
int WifiPageVisibleNetworkRowCapacity(int portrait_width,
                                      int portrait_height,
                                      const WifiPageState& state);
void DrawWifiPage(uint8_t* framebuffer,
                  int raw_width,
                  int raw_height,
                  int portrait_width,
                  int portrait_height,
                  const WifiPageState& state,
                  const StatusBarState& status_bar_state,
                  const GlobalFooterState& footer_state);

}  // namespace epaper_ui

#endif  // EPAPER_UI_WIFI_PAGE_H_
