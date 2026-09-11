#include "epaper_ui/topic_summary_page.h"

#include <algorithm>

#include "render_utils.h"

namespace epaper_ui {
namespace {

constexpr int kMargin = design::spacing::k16;
constexpr int kContentTopGap = design::spacing::k16;
constexpr int kHeadingSubtitleGap = design::spacing::k8;
constexpr int kSubtitleScrollGap = design::spacing::k16;
constexpr int kScrollButtonGap = design::spacing::k16;
constexpr int kButtonFooterGap = design::spacing::k16;
constexpr int kButtonGap = design::spacing::k16;
constexpr auto kHeadingRole = design::TypographyRole::kHeadingH1;
constexpr auto kSubtitleRole = design::TypographyRole::kLabelLarge;

int PageWidth(int portrait_width)
{
    return std::max(0, portrait_width - (2 * kMargin));
}

int FooterTop(int portrait_height)
{
    return portrait_height - design::global_footer::kBottomPadding -
           design::global_footer::kButtonSize;
}

ScrollContainerStyle ScrollStyle(int width, int panel_height)
{
    ScrollContainerStyle style = {};
    style.width = width;
    style.panel_height = panel_height;
    return style;
}

ButtonStyle MakeButtonStyle(int width, ButtonVariant variant)
{
    ButtonStyle style = {};
    style.width = width;
    style.variant = variant;
    return style;
}

struct Layout {
    UiRect heading = {};
    UiRect subtitle = {};
    UiRect scroll = {};
    UiRect back_button = {};
    UiRect get_summary_button = {};
    int scroll_panel_height = 0;
};

Layout BuildLayout(int portrait_width, int portrait_height)
{
    const int page_width = PageWidth(portrait_width);
    const int content_top = StatusBarHeight() + kContentTopGap;

    Layout layout = {};
    layout.heading = {kMargin, content_top, page_width, LineHeight(kHeadingRole)};
    layout.subtitle = {kMargin, layout.heading.bottom() + kHeadingSubtitleGap, page_width,
                       LineHeight(kSubtitleRole)};

    const int scroll_top = layout.subtitle.bottom() + kSubtitleScrollGap;
    const int footer_top = FooterTop(portrait_height);
    const int button_height = design::button::kHeight;
    const int button_row_top = std::max(scroll_top, footer_top - kButtonFooterGap - button_height);
    layout.scroll_panel_height = std::max(0, button_row_top - scroll_top - kScrollButtonGap);
    layout.scroll = {kMargin, scroll_top, page_width, layout.scroll_panel_height};

    // Back (secondary) on the left, Get/Refresh summary (primary) on the right, split evenly --
    // same row shape as Details' Back/Transcribe row.
    const int half_width = std::max(0, (page_width - kButtonGap) / 2);
    layout.back_button = {kMargin, button_row_top, half_width, button_height};
    const int right_x = kMargin + half_width + kButtonGap;
    layout.get_summary_button = {right_x, button_row_top,
                                 std::max(0, portrait_width - kMargin - right_x), button_height};
    return layout;
}

}  // namespace

void DrawTopicSummaryPage(uint8_t* framebuffer,
                          int raw_width,
                          int raw_height,
                          int portrait_width,
                          int portrait_height,
                          const TopicSummaryPageState& state,
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

    const int page_width = PageWidth(portrait_width);
    const Layout layout = BuildLayout(portrait_width, portrait_height);

    if (!state.title_text.empty()) {
        DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                           layout.heading.x, layout.heading.y, state.title_text, kHeadingRole,
                           design::color::kBlack);
    }
    if (!state.subtitle_text.empty()) {
        DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                           layout.subtitle.x, layout.subtitle.y, state.subtitle_text, kSubtitleRole,
                           design::color::kBlack);
    }

    DrawScrollContainer(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                        layout.scroll.x, layout.scroll.y, state.scroll_container,
                        ScrollStyle(page_width, layout.scroll_panel_height));

    DrawButton(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
              layout.back_button.x, layout.back_button.y, state.back_button,
              MakeButtonStyle(layout.back_button.width, ButtonVariant::kDefault));
    DrawButton(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
              layout.get_summary_button.x, layout.get_summary_button.y, state.get_summary_button,
              MakeButtonStyle(layout.get_summary_button.width, ButtonVariant::kPrimary));

    DrawGlobalFooter(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                     footer_state);
}

}  // namespace epaper_ui
