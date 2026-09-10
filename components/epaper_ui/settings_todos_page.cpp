#include "epaper_ui/settings_todos_page.h"

#include <algorithm>

#include "render_utils.h"

namespace epaper_ui {
namespace {

constexpr auto kTitleRole = design::TypographyRole::kHeadingH1;
constexpr int kSideInset = design::spacing::k16;
constexpr int kTopGap = design::spacing::k24;
constexpr int kHeadingBottomPadding = design::spacing::k24;
constexpr int kBackButtonGap = design::spacing::k24;

struct Layout {
    UiRect archive_after_input = {};
    UiRect back = {};
};

Layout BuildLayout(int portrait_width, int portrait_height, const SettingsTodosPageState& state)
{
    (void)portrait_height;
    const int page_x = kSideInset;
    const int page_width = std::max(0, portrait_width - (2 * kSideInset));
    const int title_y = StatusBarHeight() + kTopGap;
    const int title_bottom = title_y + LineHeight(kTitleRole);

    TextInputStyle archive_after_style = {};
    archive_after_style.width = page_width;
    const UiRect archive_after_input = SelectInputBounds(
        page_x, title_bottom + kHeadingBottomPadding, state.archive_after_input,
        archive_after_style);

    ButtonStyle back_style = {};
    back_style.width = page_width;
    back_style.center_label = true;
    const UiRect back =
        ButtonBounds(page_x, archive_after_input.bottom() + kBackButtonGap, state.back, back_style);

    return {
        .archive_after_input = archive_after_input,
        .back = back,
    };
}

}  // namespace

UiRect SettingsTodosPageItemBounds(int portrait_width,
                                   int portrait_height,
                                   const SettingsTodosPageState& state,
                                   SettingsTodosPageItemId item)
{
    const Layout layout = BuildLayout(portrait_width, portrait_height, state);
    switch (item) {
        case SettingsTodosPageItemId::kArchiveAfterInput:
            return layout.archive_after_input;
        case SettingsTodosPageItemId::kBack:
            return layout.back;
        case SettingsTodosPageItemId::kNone:
        default:
            return {};
    }
}

bool HitTestSettingsTodosPageItem(int portrait_width,
                                  int portrait_height,
                                  const SettingsTodosPageState& state,
                                  int x,
                                  int y,
                                  SettingsTodosPageItemId* item)
{
    if (item != nullptr) {
        *item = SettingsTodosPageItemId::kNone;
    }

    constexpr SettingsTodosPageItemId kItems[] = {
        SettingsTodosPageItemId::kArchiveAfterInput,
        SettingsTodosPageItemId::kBack,
    };
    for (SettingsTodosPageItemId candidate : kItems) {
        const UiRect bounds =
            SettingsTodosPageItemBounds(portrait_width, portrait_height, state, candidate);
        if (!bounds.IsEmpty() && bounds.Contains(x, y)) {
            if (item != nullptr) {
                *item = candidate;
            }
            return true;
        }
    }
    return false;
}

void DrawSettingsTodosPage(uint8_t* framebuffer,
                           int raw_width,
                           int raw_height,
                           int portrait_width,
                           int portrait_height,
                           const SettingsTodosPageState& state,
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

    TextInputStyle archive_after_style = {};
    archive_after_style.width = layout.archive_after_input.width;
    DrawSelectInput(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                    layout.archive_after_input.x, layout.archive_after_input.y,
                    state.archive_after_input, archive_after_style);

    DrawButton(framebuffer, raw_width, raw_height, portrait_width, portrait_height, layout.back.x,
              layout.back.y, state.back,
              {.width = layout.back.width, .center_label = true});

    DrawGlobalFooter(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                     footer_state);
}

}  // namespace epaper_ui
