#ifndef EPAPER_UI_STATUS_BAR_H_
#define EPAPER_UI_STATUS_BAR_H_

#include <cstdint>
#include <string>

namespace epaper_ui {

enum class WifiStatus : uint8_t {
    kDisconnected,
    kConnected,
    kAccessPoint,
    kDisabled,
};

struct BatteryStatus {
    int percent = -1;
    bool charging = false;
};

struct StatusBarState {
    BatteryStatus battery = {};
    WifiStatus wifi = WifiStatus::kDisabled;
    std::string time_text = {};
    bool show_gemini_icon = false;
    bool show_power_icon = false;
    bool show_sleep_icon = false;
    // Shown only while at least one recording is waiting on a transcript (offline when
    // saved, or a subsequent retry attempt still pending) -- absent otherwise, unlike a
    // permanently-visible icon with nothing to say.
    bool show_pending_transcription_icon = false;
    std::string pending_transcription_badge_text = {};
};

int StatusBarHeight();

void DrawStatusBar(uint8_t* framebuffer,
                   int raw_width,
                   int raw_height,
                   int portrait_width,
                   int portrait_height,
                   const StatusBarState& state,
                   bool draw_background = true);

}  // namespace epaper_ui

#endif  // EPAPER_UI_STATUS_BAR_H_
