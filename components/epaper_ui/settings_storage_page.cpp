#include "epaper_ui/settings_storage_page.h"

#include <algorithm>

#include "render_utils.h"

namespace epaper_ui {
namespace {

constexpr auto kTitleRole = design::TypographyRole::kHeadingH1;
constexpr int kSideInset = design::spacing::k16;
constexpr int kTopGap = design::spacing::k24;
constexpr int kHeadingBottomPadding = design::spacing::k24;
constexpr int kButtonStackGap = design::spacing::k12;
constexpr int kBackButtonGap = design::spacing::k24;

struct Layout {
    UiRect storage_status = {};
    UiRect enable_otg_button = {};
    UiRect format_sd_button = {};
    UiRect back = {};
};

Layout BuildLayout(int portrait_width, int portrait_height, const SettingsStoragePageState& state)
{
    (void)portrait_height;
    const int page_x = kSideInset;
    const int page_width = std::max(0, portrait_width - (2 * kSideInset));
    const int title_y = StatusBarHeight() + kTopGap;
    const int title_bottom = title_y + LineHeight(kTitleRole);

    SdStatusStyle storage_style = {};
    storage_style.max_width = page_width;
    const UiRect storage_status = SdStatusBounds(
        page_x, title_bottom + kHeadingBottomPadding, state.storage_status, storage_style);

    ButtonStyle otg_button_style = {};
    otg_button_style.width = page_width;
    const int button_y = storage_status.bottom() + kHeadingBottomPadding;
    const UiRect enable_otg_button =
        ButtonBounds(page_x, button_y, state.enable_otg_button, otg_button_style);

    ButtonStyle format_button_style = {};
    format_button_style.width = page_width;
    const UiRect format_sd_button =
        ButtonBounds(page_x, enable_otg_button.bottom() + kButtonStackGap, state.format_sd_button,
                     format_button_style);

    ButtonStyle back_style = {};
    back_style.width = page_width;
    back_style.center_label = true;
    const UiRect back = ButtonBounds(
        page_x, format_sd_button.bottom() + kBackButtonGap, state.back, back_style);

    return {
        .storage_status = storage_status,
        .enable_otg_button = enable_otg_button,
        .format_sd_button = format_sd_button,
        .back = back,
    };
}

}  // namespace

UiRect SettingsStoragePageItemBounds(int portrait_width,
                                     int portrait_height,
                                     const SettingsStoragePageState& state,
                                     SettingsStoragePageItemId item)
{
    const Layout layout = BuildLayout(portrait_width, portrait_height, state);
    switch (item) {
        case SettingsStoragePageItemId::kEnableOtgButton:
            return layout.enable_otg_button;
        case SettingsStoragePageItemId::kFormatSdButton:
            return layout.format_sd_button;
        case SettingsStoragePageItemId::kBack:
            return layout.back;
        case SettingsStoragePageItemId::kNone:
        default:
            return {};
    }
}

bool HitTestSettingsStoragePageItem(int portrait_width,
                                    int portrait_height,
                                    const SettingsStoragePageState& state,
                                    int x,
                                    int y,
                                    SettingsStoragePageItemId* item)
{
    if (item != nullptr) {
        *item = SettingsStoragePageItemId::kNone;
    }

    constexpr SettingsStoragePageItemId kItems[] = {
        SettingsStoragePageItemId::kEnableOtgButton,
        SettingsStoragePageItemId::kFormatSdButton,
        SettingsStoragePageItemId::kBack,
    };
    for (SettingsStoragePageItemId candidate : kItems) {
        const UiRect bounds =
            SettingsStoragePageItemBounds(portrait_width, portrait_height, state, candidate);
        if (!bounds.IsEmpty() && bounds.Contains(x, y)) {
            if (item != nullptr) {
                *item = candidate;
            }
            return true;
        }
    }
    return false;
}

void DrawSettingsStoragePage(uint8_t* framebuffer,
                             int raw_width,
                             int raw_height,
                             int portrait_width,
                             int portrait_height,
                             const SettingsStoragePageState& state,
                             const StatusBarState& status_bar_state,
                             const GlobalFooterState& footer_state)
{
    if (framebuffer == nullptr) {
        return;
    }

    FillPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                     {0, 0, portrait_width, portrait_height}, design::color::kWhite);
    DrawStatusBar(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                  status_bar_state);

    const Layout layout = BuildLayout(portrait_width, portrait_height, state);
    const int title_x = kSideInset;
    const int title_y = StatusBarHeight() + kTopGap;
    DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                       title_x, title_y, state.title_text, kTitleRole, design::color::kBlack);

    SdStatusStyle storage_style = {};
    storage_style.max_width = layout.storage_status.width;
    DrawSdStatus(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                layout.storage_status.x, layout.storage_status.y, state.storage_status,
                storage_style);

    // Outlined, like Back: OTG is a mode toggle, not a destructive action, so it should not
    // compete with Format SD for emphasis.
    ButtonStyle otg_button_style = {};
    otg_button_style.width = layout.enable_otg_button.width;
    DrawButton(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
              layout.enable_otg_button.x, layout.enable_otg_button.y, state.enable_otg_button,
              otg_button_style);

    // Primary (darker) variant: the primary action on this page.
    DrawButton(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
              layout.format_sd_button.x, layout.format_sd_button.y, state.format_sd_button,
              {.variant = ButtonVariant::kPrimary, .width = layout.format_sd_button.width});

    DrawButton(framebuffer, raw_width, raw_height, portrait_width, portrait_height, layout.back.x,
              layout.back.y, state.back,
              {.width = layout.back.width, .center_label = true});

    DrawGlobalFooter(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                     footer_state);
}

}  // namespace epaper_ui
