#include "epaper_ui/topic_entries_page.h"

#include <algorithm>

#include "render_utils.h"

namespace epaper_ui {
namespace {

constexpr int kMargin = design::spacing::k16;
constexpr int kContentTopGap = design::spacing::k16;
constexpr int kHeadingTimelineGap = design::spacing::k24;
constexpr int kTimelineButtonGap = design::spacing::k16;
constexpr int kButtonFooterGap = design::spacing::k16;
constexpr int kButtonGap = design::spacing::k16;
constexpr auto kHeadingRole = design::TypographyRole::kHeadingH1;

int PageWidth(int portrait_width)
{
    return std::max(0, portrait_width - (2 * kMargin));
}

int FooterTop(int portrait_height)
{
    return portrait_height - design::global_footer::kBottomPadding -
           design::global_footer::kButtonSize;
}

TimelineListStyle TimelineStyle(int width, int height)
{
    TimelineListStyle style = {};
    style.width = width;
    style.height = height;
    return style;
}

struct Layout {
    UiRect heading = {};
    UiRect timeline = {};
    UiRect back_button = {};
    UiRect summarize_button = {};
};

Layout BuildLayout(int portrait_width, int portrait_height, bool show_summarize)
{
    const int page_width = PageWidth(portrait_width);
    const int content_top = StatusBarHeight() + kContentTopGap;

    Layout layout = {};
    layout.heading = {kMargin, content_top, page_width, LineHeight(kHeadingRole)};

    const int timeline_top = layout.heading.bottom() + kHeadingTimelineGap;
    const int footer_top = FooterTop(portrait_height);
    const int button_height = design::button::kHeight;
    const int button_row_top = footer_top - kButtonFooterGap - button_height;
    const int timeline_height = std::max(0, button_row_top - kTimelineButtonGap - timeline_top);
    layout.timeline = {kMargin, timeline_top, page_width, timeline_height};
    if (show_summarize) {
        // Back (secondary) on the left, Summarize (primary) on the right, split evenly -- same
        // row shape as Details' Back/Transcribe row.
        const int half_width = std::max(0, (page_width - kButtonGap) / 2);
        layout.back_button = {kMargin, button_row_top, half_width, button_height};
        const int right_x = kMargin + half_width + kButtonGap;
        layout.summarize_button = {right_x, button_row_top,
                                   std::max(0, portrait_width - kMargin - right_x), button_height};
    } else {
        layout.back_button = {kMargin, button_row_top, page_width, button_height};
    }
    return layout;
}

}  // namespace

void DrawTopicEntriesPage(uint8_t* framebuffer,
                          int raw_width,
                          int raw_height,
                          int portrait_width,
                          int portrait_height,
                          const TopicEntriesPageState& state,
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

    const Layout layout = BuildLayout(portrait_width, portrait_height, state.show_summarize_button);

    if (!state.title_text.empty()) {
        DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                           layout.heading.x, layout.heading.y, state.title_text, kHeadingRole,
                           design::color::kBlack);
    }

    DrawTimelineList(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                     layout.timeline.x, layout.timeline.y, state.timeline,
                     TimelineStyle(layout.timeline.width, layout.timeline.height));

    // When Summarize is shown, Back drops to the secondary (kDefault) variant; alone it stays the
    // sole primary action.
    const ButtonVariant back_variant =
        state.show_summarize_button ? ButtonVariant::kDefault : ButtonVariant::kPrimary;
    DrawButton(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
              layout.back_button.x, layout.back_button.y, state.back_button,
              {.variant = back_variant, .width = layout.back_button.width, .center_label = true});
    if (state.show_summarize_button) {
        DrawButton(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                  layout.summarize_button.x, layout.summarize_button.y, state.summarize_button,
                  {.variant = ButtonVariant::kPrimary,
                   .width = layout.summarize_button.width,
                   .center_label = true});
    }

    DrawGlobalFooter(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                     footer_state);
}

}  // namespace epaper_ui
