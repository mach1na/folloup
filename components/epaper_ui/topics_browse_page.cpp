#include "epaper_ui/topics_browse_page.h"

#include <algorithm>

#include "render_utils.h"

namespace epaper_ui {
namespace {

constexpr auto kTitleRole = design::TypographyRole::kHeadingH1;
constexpr int kSideInset = design::spacing::k16;
constexpr int kTopGap = design::spacing::k24;
constexpr int kHeadingBottomPadding = design::spacing::k24;
constexpr int kListFooterGap = design::spacing::k16;

int FooterTop(int portrait_height)
{
    return portrait_height - design::global_footer::kBottomPadding -
           design::global_footer::kButtonSize;
}

SelectListStyle ListStyle(int width, int panel_height)
{
    SelectListStyle style = {};
    style.width = width;
    style.panel_height = panel_height;
    return style;
}

struct Layout {
    UiRect topics_list = {};
};

Layout BuildLayout(int portrait_width, int portrait_height)
{
    const int page_x = kSideInset;
    const int page_width = std::max(0, portrait_width - (2 * kSideInset));
    const int title_y = StatusBarHeight() + kTopGap;
    const int title_bottom = title_y + LineHeight(kTitleRole);
    const int list_y = title_bottom + kHeadingBottomPadding;
    const int footer_top = FooterTop(portrait_height);
    const int panel_height = std::max(0, footer_top - kListFooterGap - list_y);

    return {.topics_list = SelectListPanelBounds(page_x, list_y, ListStyle(page_width, panel_height))};
}

}  // namespace

UiRect TopicsBrowseListBounds(int portrait_width, int portrait_height,
                              const TopicsBrowsePageState&)
{
    return BuildLayout(portrait_width, portrait_height).topics_list;
}

void DrawTopicsBrowsePage(uint8_t* framebuffer,
                          int raw_width,
                          int raw_height,
                          int portrait_width,
                          int portrait_height,
                          const TopicsBrowsePageState& state,
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

    const Layout layout = BuildLayout(portrait_width, portrait_height);
    const int title_x = kSideInset;
    const int title_y = StatusBarHeight() + kTopGap;
    DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                       title_x, title_y, state.title_text, kTitleRole, design::color::kBlack);

    DrawSelectList(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                   layout.topics_list.x, layout.topics_list.y, state.topics_list,
                   ListStyle(layout.topics_list.width, layout.topics_list.height));

    DrawGlobalFooter(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                     footer_state);
}

}  // namespace epaper_ui
