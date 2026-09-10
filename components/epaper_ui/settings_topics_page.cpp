#include "epaper_ui/settings_topics_page.h"

#include <algorithm>

#include "render_utils.h"

namespace epaper_ui {
namespace {

constexpr auto kTitleRole = design::TypographyRole::kHeadingH1;
constexpr int kSideInset = design::spacing::k16;
constexpr int kTopGap = design::spacing::k24;
constexpr int kHeadingBottomPadding = design::spacing::k24;
constexpr int kListButtonGap = design::spacing::k24;
constexpr int kButtonStackGap = design::spacing::k12;
constexpr int kFooterButtonGap = design::spacing::k32;

struct Layout {
    UiRect topics_list = {};
    UiRect new_topic_button = {};
    UiRect back = {};
};

SelectListStyle ListStyle(int width, int panel_height)
{
    SelectListStyle style = {};
    style.width = width;
    style.panel_height = panel_height;
    return style;
}

Layout BuildLayout(int portrait_width, int portrait_height, const SettingsTopicsPageState& state)
{
    const int page_x = kSideInset;
    const int page_width = std::max(0, portrait_width - (2 * kSideInset));
    const int title_y = StatusBarHeight() + kTopGap;
    const int title_bottom = title_y + LineHeight(kTitleRole);
    const int list_y = title_bottom + kHeadingBottomPadding;
    // The bottom of the content area, above the global footer's icon row -- same calculation
    // wifi_page.cpp uses for its own bottom-anchored Back button, since the footer is drawn as
    // its own fixed bottom bar rather than reserving space the page layout already knows about.
    const int footer_top = portrait_height - design::global_footer::kBottomPadding -
                           design::global_footer::kButtonSize;

    ButtonStyle back_style = {};
    back_style.width = page_width;
    back_style.center_label = true;
    ButtonStyle new_topic_style = {};
    new_topic_style.width = page_width;
    new_topic_style.center_label = true;

    // Measure the two bottom buttons first (fixed height), then let the list fill whatever
    // vertical space remains above them -- same technique wifi_page.cpp uses for its network
    // list, so the list scrolls internally instead of the page growing unbounded.
    const UiRect measured_back = ButtonBounds(page_x, 0, state.back, back_style);
    const UiRect measured_new_topic =
        ButtonBounds(page_x, 0, state.new_topic_button, new_topic_style);
    const int back_y = footer_top - kFooterButtonGap - measured_back.height;
    const int new_topic_y = back_y - kButtonStackGap - measured_new_topic.height;
    const int list_bottom = new_topic_y - kListButtonGap;

    const UiRect topics_list =
        SelectListPanelBounds(page_x, list_y, ListStyle(page_width, std::max(0, list_bottom - list_y)));
    const UiRect new_topic_button =
        ButtonBounds(page_x, new_topic_y, state.new_topic_button, new_topic_style);
    const UiRect back = ButtonBounds(page_x, back_y, state.back, back_style);

    return {
        .topics_list = topics_list,
        .new_topic_button = new_topic_button,
        .back = back,
    };
}

}  // namespace

UiRect SettingsTopicsListBounds(int portrait_width, int portrait_height,
                                const SettingsTopicsPageState& state)
{
    return BuildLayout(portrait_width, portrait_height, state).topics_list;
}

UiRect SettingsTopicsNewTopicButtonBounds(int portrait_width, int portrait_height,
                                          const SettingsTopicsPageState& state)
{
    return BuildLayout(portrait_width, portrait_height, state).new_topic_button;
}

UiRect SettingsTopicsBackButtonBounds(int portrait_width, int portrait_height,
                                      const SettingsTopicsPageState& state)
{
    return BuildLayout(portrait_width, portrait_height, state).back;
}

void DrawSettingsTopicsPage(uint8_t* framebuffer,
                            int raw_width,
                            int raw_height,
                            int portrait_width,
                            int portrait_height,
                            const SettingsTopicsPageState& state,
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

    DrawSelectList(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                   layout.topics_list.x, layout.topics_list.y, state.topics_list,
                   ListStyle(layout.topics_list.width, layout.topics_list.height));

    // Outlined, like Back: creating a topic is not the primary reason to visit this page once
    // topics already exist, so it should not compete visually with a focused row.
    ButtonStyle new_topic_style = {};
    new_topic_style.width = layout.new_topic_button.width;
    new_topic_style.center_label = true;
    DrawButton(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
              layout.new_topic_button.x, layout.new_topic_button.y, state.new_topic_button,
              new_topic_style);

    DrawButton(framebuffer, raw_width, raw_height, portrait_width, portrait_height, layout.back.x,
              layout.back.y, state.back,
              {.width = layout.back.width, .center_label = true});

    DrawGlobalFooter(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                     footer_state);
}

}  // namespace epaper_ui
