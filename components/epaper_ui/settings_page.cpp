#include "epaper_ui/settings_page.h"

#include <algorithm>
#include <array>

#include "render_utils.h"

namespace epaper_ui {
namespace {

constexpr auto kTitleRole = design::TypographyRole::kHeadingH1;
constexpr auto kSectionRole = design::TypographyRole::kHeadingH2;
constexpr int kSideInset = design::spacing::k16;
constexpr int kTopGap = design::spacing::k16;
constexpr int kTitleBottomGap = design::spacing::k24;
constexpr int kNetworkHeadingGap = 0;
constexpr int kSectionGap = design::spacing::k24;
constexpr int kStorageStatusGap = design::spacing::k12;
constexpr int kStorageButtonTopGap = design::spacing::k16;
constexpr int kButtonStackGap = design::spacing::k12;
constexpr int kTodosHeadingGap = 0;
// Gap between the last scrollable item and the footer, mirroring notes_page.cpp's
// kTimelineFooterGap convention for the same "content viewport bottom" concept.
constexpr int kContentFooterGap = design::spacing::k16;
constexpr int kScrollbarWidth = design::scroll_container::kScrollbarWidth;

// The 6 focusable items, in stacking order -- matches SettingsPageItemId/HitTestSettingsPageItem's
// kItems order below.
constexpr int kItemCount = 6;

struct Layout {
    UiRect wifi_toggle = {};
    UiRect access_point_toggle = {};
    UiRect storage_status = {};
    UiRect enable_otg_button = {};
    UiRect format_sd_button = {};
    UiRect manual_onboarding_button = {};
    UiRect archive_after_input = {};
};

// Where the title sits, and where the scrollable content below it starts -- both independent of
// state/width/height, so callers that only need the viewport bounds don't have to build a layout.
int TitleY() { return StatusBarHeight() + kTopGap; }
int ContentTop() { return TitleY() + LineHeight(kTitleRole) + kTitleBottomGap; }
int FooterTop(int portrait_height)
{
    return portrait_height - design::global_footer::kBottomPadding -
           design::global_footer::kButtonSize;
}
int ContentBottom(int portrait_height) { return FooterTop(portrait_height) - kContentFooterGap; }

// One item's on-screen span (in unscrolled/"virtual" coordinates), with `top` pulled up to
// include that item's section heading when it starts one -- so a heading never gets separated
// from the item below it once scrolling is applied. `focused` mirrors whichever single item's own
// state field currently reports focus (exactly one is expected true at a time, matching roving
// focus semantics).
struct ItemBlock {
    int top = 0;
    int bottom = 0;
    bool focused = false;
};

bool ToggleFocused(ToggleVisualState state)
{
    return state == ToggleVisualState::kFocusOn || state == ToggleVisualState::kFocusOff;
}

// Virtual (unscrolled) layout -- the stacking math is unchanged from before scrolling existed.
// Scroll offset is applied by the public BuildLayout() below, once, as a final pass.
Layout BuildLayoutCore(int portrait_width, int portrait_height, const SettingsPageState& state)
{
    (void)portrait_height;
    const int page_x = kSideInset;
    const int page_width = std::max(0, portrait_width - (2 * kSideInset));

    const int network_heading_y = ContentTop();
    const int network_items_y =
        network_heading_y + LineHeight(kSectionRole) + kNetworkHeadingGap;

    MenuToggleStyle network_toggle_style = {};
    network_toggle_style.width = page_width;
    const UiRect wifi_toggle = MenuToggleBounds(page_x, network_items_y, network_toggle_style);

    MenuToggleStyle access_point_style = network_toggle_style;
    access_point_style.bottom_border_thickness = 0;
    const UiRect access_point_toggle =
        MenuToggleBounds(page_x, wifi_toggle.bottom(), access_point_style);

    const int storage_heading_y = access_point_toggle.bottom() + kSectionGap;
    SdStatusStyle storage_style = {};
    storage_style.max_width = page_width;
    const UiRect storage_status = SdStatusBounds(page_x,
                                                 storage_heading_y + LineHeight(kSectionRole) +
                                                     kStorageStatusGap,
                                                 state.storage_status,
                                                 storage_style);

    ButtonStyle otg_button_style = {};
    otg_button_style.width = page_width;
    const int button_y = storage_status.bottom() + kStorageButtonTopGap;
    const UiRect enable_otg_button =
        ButtonBounds(page_x, button_y, state.enable_otg_button, otg_button_style);

    ButtonStyle format_button_style = {};
    format_button_style.width = page_width;
    const UiRect format_sd_button =
        ButtonBounds(page_x, enable_otg_button.bottom() + kButtonStackGap,
                     state.format_sd_button, format_button_style);

    ButtonStyle manual_button_style = {};
    manual_button_style.width = page_width;
    const UiRect manual_onboarding_button =
        ButtonBounds(page_x, format_sd_button.bottom() + kButtonStackGap,
                     state.manual_onboarding_button, manual_button_style);

    const int todos_heading_y = manual_onboarding_button.bottom() + kSectionGap;
    TextInputStyle archive_after_style = {};
    archive_after_style.width = page_width;
    const UiRect archive_after_input =
        SelectInputBounds(page_x, todos_heading_y + LineHeight(kSectionRole) + kTodosHeadingGap,
                          state.archive_after_input, archive_after_style);

    return {
        .wifi_toggle = wifi_toggle,
        .access_point_toggle = access_point_toggle,
        .storage_status = storage_status,
        .enable_otg_button = enable_otg_button,
        .format_sd_button = format_sd_button,
        .manual_onboarding_button = manual_onboarding_button,
        .archive_after_input = archive_after_input,
    };
}

// storage_status has no NavigationItemRole of its own (it's a read-only status row, not a
// focusable item) -- its heading and body fold into enable_otg_button's block below, so the
// whole "Storage" section always scrolls in as one unit rather than getting split partway.
std::array<ItemBlock, kItemCount> ComputeBlocks(const Layout& virtual_layout,
                                                const SettingsPageState& state)
{
    const int storage_heading_top = virtual_layout.access_point_toggle.bottom() + kSectionGap;
    const int todos_heading_top = virtual_layout.manual_onboarding_button.bottom() + kSectionGap;
    return {{
        {ContentTop(), virtual_layout.wifi_toggle.bottom(),
         ToggleFocused(state.wifi_toggle.toggle_state)},
        {virtual_layout.wifi_toggle.bottom(), virtual_layout.access_point_toggle.bottom(),
         ToggleFocused(state.access_point_toggle.toggle_state)},
        {storage_heading_top, virtual_layout.enable_otg_button.bottom(),
         state.enable_otg_button.selected},
        {virtual_layout.enable_otg_button.bottom(), virtual_layout.format_sd_button.bottom(),
         state.format_sd_button.selected},
        {virtual_layout.format_sd_button.bottom(), virtual_layout.manual_onboarding_button.bottom(),
         state.manual_onboarding_button.selected},
        {todos_heading_top, virtual_layout.archive_after_input.bottom(),
         state.archive_after_input.focused},
    }};
}

int FocusedBlockIndex(const std::array<ItemBlock, kItemCount>& blocks)
{
    for (int index = 0; index < kItemCount; ++index) {
        if (blocks[static_cast<size_t>(index)].focused) {
            return index;
        }
    }
    return -1;
}

// Does the span from blocks[anchor].top to blocks[last].bottom fit within viewport_height?
bool RangeFits(const std::array<ItemBlock, kItemCount>& blocks, int anchor, int last,
              int viewport_height)
{
    return (blocks[static_cast<size_t>(last)].bottom - blocks[static_cast<size_t>(anchor)].top) <=
           viewport_height;
}

int ResolveScrollOffsetY(int portrait_height, const Layout& virtual_layout,
                         const SettingsPageState& state)
{
    const std::array<ItemBlock, kItemCount> blocks = ComputeBlocks(virtual_layout, state);
    const int content_top = ContentTop();
    const int content_bottom = ContentBottom(portrait_height);
    const int viewport_height = std::max(0, content_bottom - content_top);
    const int total_content_height =
        std::max(0, blocks[kItemCount - 1].bottom - content_top);
    const int max_offset = std::max(0, total_content_height - viewport_height);

    const int anchor = std::clamp(state.visible_item_index, 0, kItemCount - 1);
    return std::clamp(blocks[static_cast<size_t>(anchor)].top - content_top, 0, max_offset);
}

Layout ApplyScrollOffset(Layout layout, int offset)
{
    const auto shift = [offset](UiRect rect) {
        rect.y -= offset;
        return rect;
    };
    layout.wifi_toggle = shift(layout.wifi_toggle);
    layout.access_point_toggle = shift(layout.access_point_toggle);
    layout.storage_status = shift(layout.storage_status);
    layout.enable_otg_button = shift(layout.enable_otg_button);
    layout.format_sd_button = shift(layout.format_sd_button);
    layout.manual_onboarding_button = shift(layout.manual_onboarding_button);
    layout.archive_after_input = shift(layout.archive_after_input);
    return layout;
}

// Screen-space layout: the virtual (unscrolled) stack, shifted so the currently-resolved
// visible_item_index anchor sits at the top of the content viewport. Assumes
// state.visible_item_index has already been resolved (see SettingsPageResolveVisibleAnchor) --
// this just applies it, cheaply, on every render.
Layout BuildLayout(int portrait_width, int portrait_height, const SettingsPageState& state)
{
    const Layout virtual_layout = BuildLayoutCore(portrait_width, portrait_height, state);
    const int offset = ResolveScrollOffsetY(portrait_height, virtual_layout, state);
    return ApplyScrollOffset(virtual_layout, offset);
}

bool FullyVisible(const UiRect& rect, int content_top, int content_bottom)
{
    return rect.y >= content_top && rect.bottom() <= content_bottom;
}

}  // namespace

int SettingsPageResolveVisibleAnchor(int portrait_width,
                                     int portrait_height,
                                     const SettingsPageState& state)
{
    const Layout virtual_layout = BuildLayoutCore(portrait_width, portrait_height, state);
    const std::array<ItemBlock, kItemCount> blocks = ComputeBlocks(virtual_layout, state);
    const int focused_index = FocusedBlockIndex(blocks);
    const int current_anchor = std::clamp(state.visible_item_index, 0, kItemCount - 1);
    if (focused_index < 0) {
        return current_anchor;
    }
    if (focused_index < current_anchor) {
        // Scrolled past the anchor going up -- reveal it directly at the top.
        return focused_index;
    }

    const int viewport_height =
        std::max(0, ContentBottom(portrait_height) - ContentTop());
    int anchor = current_anchor;
    while (anchor < focused_index && !RangeFits(blocks, anchor, focused_index, viewport_height)) {
        ++anchor;
    }
    return anchor;
}

UiRect SettingsPageItemBounds(int portrait_width,
                              int portrait_height,
                              const SettingsPageState& state,
                              SettingsPageItemId item)
{
    const Layout layout = BuildLayout(portrait_width, portrait_height, state);
    switch (item) {
        case SettingsPageItemId::kWifiToggle:
            return layout.wifi_toggle;
        case SettingsPageItemId::kAccessPointToggle:
            return layout.access_point_toggle;
        case SettingsPageItemId::kEnableOtgButton:
            return layout.enable_otg_button;
        case SettingsPageItemId::kFormatSdButton:
            return layout.format_sd_button;
        case SettingsPageItemId::kManualOnboardingButton:
            return layout.manual_onboarding_button;
        case SettingsPageItemId::kArchiveAfterInput:
            return layout.archive_after_input;
        case SettingsPageItemId::kNone:
        default:
            return {};
    }
}

UiRect SettingsPageItemVisualBounds(int portrait_width,
                                    int portrait_height,
                                    const SettingsPageState& state,
                                    SettingsPageItemId item)
{
    return SettingsPageItemBounds(portrait_width, portrait_height, state, item);
}

bool HitTestSettingsPageItem(int portrait_width,
                             int portrait_height,
                             const SettingsPageState& state,
                             int x,
                             int y,
                             SettingsPageItemId* item)
{
    if (item != nullptr) {
        *item = SettingsPageItemId::kNone;
    }

    constexpr SettingsPageItemId kItems[] = {
        SettingsPageItemId::kWifiToggle,
        SettingsPageItemId::kAccessPointToggle,
        SettingsPageItemId::kEnableOtgButton,
        SettingsPageItemId::kFormatSdButton,
        SettingsPageItemId::kManualOnboardingButton,
        SettingsPageItemId::kArchiveAfterInput,
    };
    const int content_top = ContentTop();
    const int content_bottom = ContentBottom(portrait_height);
    for (SettingsPageItemId candidate : kItems) {
        const UiRect bounds =
            SettingsPageItemBounds(portrait_width, portrait_height, state, candidate);
        if (!bounds.IsEmpty() && FullyVisible(bounds, content_top, content_bottom) &&
            bounds.Contains(x, y)) {
            if (item != nullptr) {
                *item = candidate;
            }
            return true;
        }
    }

    return false;
}

void DrawSettingsPage(uint8_t* framebuffer,
                      int raw_width,
                      int raw_height,
                      int portrait_width,
                      int portrait_height,
                      const SettingsPageState& state,
                      const StatusBarState& status_bar_state,
                      const GlobalFooterState& footer_state)
{
    if (framebuffer == nullptr) {
        return;
    }

    FillPortraitRect(framebuffer,
                     raw_width,
                     raw_height,
                     portrait_width,
                     portrait_height,
                     {0, 0, portrait_width, portrait_height},
                     design::color::kWhite);
    DrawStatusBar(framebuffer,
                  raw_width,
                  raw_height,
                  portrait_width,
                  portrait_height,
                  status_bar_state);

    const Layout layout = BuildLayout(portrait_width, portrait_height, state);
    const int content_top = ContentTop();
    const int content_bottom = ContentBottom(portrait_height);
    // Nothing partially within the content viewport is drawn -- there's no pixel clip primitive
    // in this rendering pipeline, so a widget or heading either fits entirely or is skipped for
    // this frame, matching how timeline_list.cpp/scroll_container.cpp already handle overflow.
    const auto visible = [&](const UiRect& rect) { return FullyVisible(rect, content_top, content_bottom); };

    const int title_x = kSideInset;
    const int title_y = TitleY();
    DrawTypographyText(framebuffer,
                       raw_width,
                       raw_height,
                       portrait_width,
                       portrait_height,
                       title_x,
                       title_y,
                       state.title_text,
                       kTitleRole,
                       design::color::kBlack);

    const UiRect network_heading = {
        title_x, layout.wifi_toggle.y - kNetworkHeadingGap - LineHeight(kSectionRole),
        layout.wifi_toggle.width, LineHeight(kSectionRole)};
    if (visible(network_heading)) {
        DrawTypographyText(framebuffer,
                           raw_width,
                           raw_height,
                           portrait_width,
                           portrait_height,
                           title_x,
                           network_heading.y,
                           "Network",
                           kSectionRole,
                           design::color::kBlack);
    }

    MenuToggleStyle wifi_style = {};
    wifi_style.width = layout.wifi_toggle.width;
    wifi_style.height = layout.wifi_toggle.height;
    if (visible(layout.wifi_toggle)) {
        DrawMenuToggle(framebuffer,
                       raw_width,
                       raw_height,
                       portrait_width,
                       portrait_height,
                       layout.wifi_toggle.x,
                       layout.wifi_toggle.y,
                       state.wifi_toggle,
                       wifi_style);
    }

    MenuToggleStyle access_point_style = wifi_style;
    access_point_style.bottom_border_thickness = 0;
    if (visible(layout.access_point_toggle)) {
        DrawMenuToggle(framebuffer,
                       raw_width,
                       raw_height,
                       portrait_width,
                       portrait_height,
                       layout.access_point_toggle.x,
                       layout.access_point_toggle.y,
                       state.access_point_toggle,
                       access_point_style);
    }

    const UiRect storage_heading = {
        title_x, layout.storage_status.y - kStorageStatusGap - LineHeight(kSectionRole),
        layout.storage_status.width, LineHeight(kSectionRole)};
    if (visible(storage_heading)) {
        DrawTypographyText(framebuffer,
                           raw_width,
                           raw_height,
                           portrait_width,
                           portrait_height,
                           title_x,
                           storage_heading.y,
                           "Storage",
                           kSectionRole,
                           design::color::kBlack);
    }

    SdStatusStyle storage_style = {};
    storage_style.max_width = layout.storage_status.width;
    if (visible(layout.storage_status)) {
        DrawSdStatus(framebuffer,
                     raw_width,
                     raw_height,
                     portrait_width,
                     portrait_height,
                     layout.storage_status.x,
                     layout.storage_status.y,
                     state.storage_status,
                     storage_style);
    }

    // Outlined, like Manual onboarding: OTG is a mode toggle, not a destructive action, so
    // it should not compete with Format SD for emphasis.
    ButtonStyle otg_button_style = {};
    otg_button_style.width = layout.enable_otg_button.width;
    if (visible(layout.enable_otg_button)) {
        DrawButton(framebuffer,
                   raw_width,
                   raw_height,
                   portrait_width,
                   portrait_height,
                   layout.enable_otg_button.x,
                   layout.enable_otg_button.y,
                   state.enable_otg_button,
                   otg_button_style);
    }

    ButtonStyle format_button_style = {};
    format_button_style.width = layout.format_sd_button.width;
    // Primary action on the page -> primary (darker) variant.
    format_button_style.variant = ButtonVariant::kPrimary;
    if (visible(layout.format_sd_button)) {
        DrawButton(framebuffer,
                   raw_width,
                   raw_height,
                   portrait_width,
                   portrait_height,
                   layout.format_sd_button.x,
                   layout.format_sd_button.y,
                   state.format_sd_button,
                   format_button_style);
    }

    // Secondary utility action -> default (outlined) variant so it reads below Format SD.
    ButtonStyle manual_button_style = {};
    manual_button_style.width = layout.manual_onboarding_button.width;
    if (visible(layout.manual_onboarding_button)) {
        DrawButton(framebuffer,
                   raw_width,
                   raw_height,
                   portrait_width,
                   portrait_height,
                   layout.manual_onboarding_button.x,
                   layout.manual_onboarding_button.y,
                   state.manual_onboarding_button,
                   manual_button_style);
    }

    const UiRect todos_heading = {
        title_x, layout.archive_after_input.y - kTodosHeadingGap - LineHeight(kSectionRole),
        layout.archive_after_input.width, LineHeight(kSectionRole)};
    if (visible(todos_heading)) {
        DrawTypographyText(framebuffer,
                           raw_width,
                           raw_height,
                           portrait_width,
                           portrait_height,
                           title_x,
                           todos_heading.y,
                           "Todos",
                           kSectionRole,
                           design::color::kBlack);
    }

    TextInputStyle archive_after_style = {};
    archive_after_style.width = layout.archive_after_input.width;
    if (visible(layout.archive_after_input)) {
        DrawSelectInput(framebuffer,
                        raw_width,
                        raw_height,
                        portrait_width,
                        portrait_height,
                        layout.archive_after_input.x,
                        layout.archive_after_input.y,
                        state.archive_after_input,
                        archive_after_style);
    }

    // Scrollbar: only when content actually overflows the viewport.
    const Layout virtual_layout = BuildLayoutCore(portrait_width, portrait_height, state);
    const std::array<ItemBlock, kItemCount> blocks = ComputeBlocks(virtual_layout, state);
    const int viewport_height = std::max(0, content_bottom - content_top);
    const int total_content_height =
        std::max(0, blocks[kItemCount - 1].bottom - content_top);
    if (total_content_height > viewport_height && viewport_height > 0) {
        const int offset = ResolveScrollOffsetY(portrait_height, virtual_layout, state);
        const int max_offset = std::max(0, total_content_height - viewport_height);
        // Flush to the screen edge, inside the existing kSideInset margin (content's own right
        // edge sits at portrait_width - kSideInset, leaving a gap before the scrollbar) so it
        // doesn't overlap any full-width widget.
        const UiRect track = {portrait_width - kScrollbarWidth, content_top, kScrollbarWidth,
                              viewport_height};
        FillPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                         track, design::color::kScrollbarTrack);
        const int thumb_height = std::clamp(
            (viewport_height * viewport_height) / total_content_height,
            std::min(viewport_height, design::scroll_container::kMinThumbHeight), viewport_height);
        const int travel = std::max(0, viewport_height - thumb_height);
        const int thumb_y =
            track.y + (max_offset > 0 ? (travel * offset + (max_offset / 2)) / max_offset : 0);
        const UiRect thumb = {track.x, thumb_y, kScrollbarWidth, thumb_height};
        FillPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                         thumb, design::color::kScrollbarThumb);
    }

    DrawGlobalFooter(framebuffer,
                     raw_width,
                     raw_height,
                     portrait_width,
                     portrait_height,
                     footer_state);
}

}  // namespace epaper_ui
