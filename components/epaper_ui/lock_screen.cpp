#include "epaper_ui/lock_screen.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "design_tokens.h"
#include "epaper_ui/checkbox.h"
#include "epaper_ui/font_renderer.h"
#include "project_assets.h"
#include "render_utils.h"

namespace epaper_ui {
namespace {

template <typename DrawFn>
void DrawOutlined(DrawFn&& draw_fn, int stroke_thickness)
{
    const int thickness = std::max(0, stroke_thickness);
    for (int dy = -thickness; dy <= thickness; ++dy) {
        for (int dx = -thickness; dx <= thickness; ++dx) {
            if (dx == 0 && dy == 0) {
                continue;
            }
            draw_fn(dx, dy, design::color::kWhite);
        }
    }
    draw_fn(0, 0, design::color::kBlack);
}

void DrawOutlinedText(uint8_t* framebuffer,
                      int raw_width,
                      int raw_height,
                      int portrait_width,
                      int portrait_height,
                      int x,
                      int y,
                      std::string_view text,
                      design::TypographyRole role,
                      int stroke_thickness)
{
    DrawOutlined(
        [&](int dx, int dy, uint8_t tone) {
            DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                               x + dx, y + dy, text, role, tone);
        },
        stroke_thickness);
}

void DrawOutlinedAsset(uint8_t* framebuffer,
                       int raw_width,
                       int raw_height,
                       int portrait_width,
                       int portrait_height,
                       int x,
                       int y,
                       const EmbeddedImageAsset* asset,
                       int stroke_thickness)
{
    if (asset == nullptr) {
        return;
    }

    DrawOutlined(
        [&](int dx, int dy, uint8_t tone) {
            DrawPortraitMonoAsset(framebuffer,
                                  raw_width,
                                  raw_height,
                                  portrait_width,
                                  portrait_height,
                                  x + dx,
                                  y + dy,
                                  asset,
                                  tone);
        },
        stroke_thickness);
}

const EmbeddedImageAsset* ResolveBatteryIcon(const BatteryStatus& battery)
{
    if (battery.charging) {
        return project_assets::GetIcon(EmbeddedIconId::kBattery6);
    }
    if (battery.percent < 0 || battery.percent <= 10) {
        return project_assets::GetIcon(EmbeddedIconId::kBattery1);
    }
    if (battery.percent <= 30) {
        return project_assets::GetIcon(EmbeddedIconId::kBattery2);
    }
    if (battery.percent <= 50) {
        return project_assets::GetIcon(EmbeddedIconId::kBattery3);
    }
    if (battery.percent <= 75) {
        return project_assets::GetIcon(EmbeddedIconId::kBattery4);
    }
    return project_assets::GetIcon(EmbeddedIconId::kBattery5);
}

const EmbeddedImageAsset* ResolveWifiIcon(WifiStatus status)
{
    switch (status) {
        case WifiStatus::kDisconnected:
            return project_assets::GetIcon(EmbeddedIconId::kWifi3);
        case WifiStatus::kConnected:
            return project_assets::GetIcon(EmbeddedIconId::kWifi2);
        case WifiStatus::kAccessPoint:
            return project_assets::GetIcon(EmbeddedIconId::kWifi4);
        case WifiStatus::kDisabled:
        default:
            return project_assets::GetIcon(EmbeddedIconId::kWifi1);
    }
}

const EmbeddedImageAsset* ResolveGeminiIcon(bool visible)
{
    if (!visible) {
        return nullptr;
    }
    return project_assets::GetIcon(EmbeddedIconId::kStar);
}

int CenterY(int container_top, int container_height, int item_height)
{
    return container_top + std::max(0, (container_height - item_height) / 2);
}

void DrawIconSlot(uint8_t* framebuffer,
                  int raw_width,
                  int raw_height,
                  int portrait_width,
                  int portrait_height,
                  int slot_x,
                  int top,
                  int box_size,
                  const EmbeddedImageAsset* asset,
                  int stroke_thickness)
{
    if (asset == nullptr) {
        return;
    }

    const int draw_x = slot_x + std::max(0, (box_size - static_cast<int>(asset->width)) / 2);
    const int draw_y = top + std::max(0, (box_size - static_cast<int>(asset->height)) / 2);
    DrawOutlinedAsset(framebuffer,
                      raw_width,
                      raw_height,
                      portrait_width,
                      portrait_height,
                      draw_x,
                      draw_y,
                      asset,
                      stroke_thickness);
}

void DrawLockScreenStatusRow(uint8_t* framebuffer,
                             int raw_width,
                             int raw_height,
                             int portrait_width,
                             int portrait_height,
                             const StatusBarState& state)
{
    const int bar_height = design::status_bar::kHeight;
    const int content_right = portrait_width - design::status_bar::kSidePadding;
    const int icon_box = design::status_bar::kPreferredIconSize;
    const int icon_top = CenterY(0, bar_height, icon_box);
    const auto label_role = design::TypographyRole::kStatusBarLabel;
    const int text_height = LineHeight(label_role);
    const int text_y = CenterY(0, bar_height, text_height);
    const int stroke_thickness = design::status_bar::kStrokeThickness;

    std::string battery_text;
    if (state.battery.percent >= 0) {
        battery_text = std::to_string(std::clamp(state.battery.percent, 0, 100)) + "%";
    }
    const int battery_text_width = MeasureText(label_role, battery_text);

    const EmbeddedImageAsset* battery_icon = ResolveBatteryIcon(state.battery);
    const EmbeddedImageAsset* wifi_icon = ResolveWifiIcon(state.wifi);
    const EmbeddedImageAsset* gemini_icon = ResolveGeminiIcon(state.show_gemini_icon);
    const EmbeddedImageAsset* sleep_icon =
        state.show_sleep_icon ? project_assets::GetIcon(EmbeddedIconId::kSleep) : nullptr;
    const EmbeddedImageAsset* power_icon =
        state.show_power_icon ? project_assets::GetIcon(EmbeddedIconId::kPower) : nullptr;

    int cursor_right = content_right;
    if (battery_text_width > 0) {
        cursor_right -= battery_text_width;
        DrawOutlinedText(framebuffer,
                         raw_width,
                         raw_height,
                         portrait_width,
                         portrait_height,
                         cursor_right,
                         text_y,
                         battery_text,
                         label_role,
                         stroke_thickness);
    }

    cursor_right -= design::status_bar::kItemGap + icon_box;
    DrawIconSlot(framebuffer,
                 raw_width,
                 raw_height,
                 portrait_width,
                 portrait_height,
                 cursor_right,
                 icon_top,
                 icon_box,
                 battery_icon,
                 stroke_thickness);

    cursor_right -= design::status_bar::kItemGap + icon_box;
    DrawIconSlot(framebuffer,
                 raw_width,
                 raw_height,
                 portrait_width,
                 portrait_height,
                 cursor_right,
                 icon_top,
                 icon_box,
                 wifi_icon,
                 stroke_thickness);

    if (state.show_gemini_icon) {
        cursor_right -= design::status_bar::kItemGap + icon_box;
        DrawIconSlot(framebuffer,
                     raw_width,
                     raw_height,
                     portrait_width,
                     portrait_height,
                     cursor_right,
                     icon_top,
                     icon_box,
                     gemini_icon,
                     stroke_thickness);
    }

    if (state.show_sleep_icon) {
        cursor_right -= design::status_bar::kItemGap + icon_box;
        DrawIconSlot(framebuffer,
                     raw_width,
                     raw_height,
                     portrait_width,
                     portrait_height,
                     cursor_right,
                     icon_top,
                     icon_box,
                     sleep_icon,
                     stroke_thickness);
    }

    if (state.show_power_icon) {
        cursor_right -= design::status_bar::kItemGap + icon_box;
        DrawIconSlot(framebuffer,
                     raw_width,
                     raw_height,
                     portrait_width,
                     portrait_height,
                     cursor_right,
                     icon_top,
                     icon_box,
                     power_icon,
                     stroke_thickness);
    }
}

std::string_view SafeText(const std::string& text, std::string_view fallback)
{
    return text.empty() ? fallback : std::string_view(text);
}

std::string BuildDateLine(std::string_view weekday, std::string_view date)
{
    if (weekday.empty()) {
        return std::string(date);
    }
    if (date.empty()) {
        return std::string(weekday);
    }
    return std::string(weekday) + ", " + std::string(date);
}

// Word-wraps `text` to `max_width` at the body role, capped to
// design::lock_screen::kMaxWrapLinesPerTodo lines -- the last one ellipsis-fitted from
// whatever text didn't make the earlier lines, rather than just cutting off wherever the
// word-wrap happened to land.
std::vector<std::string> WrapAndCapTodoText(const std::string& text, int max_width)
{
    const auto role = design::TypographyRole::kBody;
    std::vector<std::string> lines = WrapTextToWidth(role, text, max_width);
    const size_t max_lines = static_cast<size_t>(design::lock_screen::kMaxWrapLinesPerTodo);
    if (lines.size() <= max_lines || max_lines == 0) {
        return lines;
    }

    std::string tail = lines[max_lines - 1];
    for (size_t i = max_lines; i < lines.size(); ++i) {
        tail += " " + lines[i];
    }
    lines.resize(max_lines);
    lines[max_lines - 1] = FitLabelText(role, tail, max_width);
    return lines;
}

// Draws one checkbox + wrapped-text todo row at (x, y) within `content_width`. Returns the
// row's total drawn height so the caller can advance its layout cursor.
int DrawTodoRow(uint8_t* framebuffer,
                int raw_width,
                int raw_height,
                int portrait_width,
                int portrait_height,
                int x,
                int y,
                int content_width,
                const std::string& text)
{
    const int checkbox_size = design::lock_screen::kCheckboxSize;
    const int text_x = x + checkbox_size + design::lock_screen::kCheckboxTextGap;
    const int text_max_width =
        std::max(0, content_width - checkbox_size - design::lock_screen::kCheckboxTextGap);
    const std::vector<std::string> lines = WrapAndCapTodoText(text, text_max_width);

    const CheckboxState checkbox_state = {.checked = false, .selected = false};
    CheckboxStyle checkbox_style = {};
    checkbox_style.size = checkbox_size;
    DrawCheckbox(framebuffer, raw_width, raw_height, portrait_width, portrait_height, x, y,
                checkbox_state, checkbox_style);

    const auto role = design::TypographyRole::kBody;
    const int line_height = LineHeight(role);
    int line_y = y;
    for (const std::string& line : lines) {
        DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                           text_x, line_y, line, role, design::color::kBlack);
        line_y += line_height + design::lock_screen::kWrapLineGap;
    }

    const int text_height =
        lines.empty() ? 0
                     : (static_cast<int>(lines.size()) * line_height) +
                           ((static_cast<int>(lines.size()) - 1) * design::lock_screen::kWrapLineGap);
    return std::max(checkbox_size, text_height);
}

}  // namespace

void DrawLockScreen(uint8_t* framebuffer,
                    int raw_width,
                    int raw_height,
                    int portrait_width,
                    int portrait_height,
                    const LockScreenState& state,
                    const StatusBarState& status_state)
{
    if (framebuffer == nullptr) {
        return;
    }

    DrawLockScreenStatusRow(framebuffer,
                            raw_width,
                            raw_height,
                            portrait_width,
                            portrait_height,
                            status_state);

    const int content_left = design::lock_screen::kSidePadding;
    const int content_width =
        std::max(0, portrait_width - (2 * design::lock_screen::kSidePadding));
    int cursor_y = design::lock_screen::kContentTop;

    const std::string date_line =
        BuildDateLine(SafeText(state.weekday_text, ""), SafeText(state.date_text, ""));
    if (!date_line.empty()) {
        const auto date_role = design::TypographyRole::kLabelMediumBlack;
        const int date_width = MeasureText(date_role, date_line);
        DrawTypographyText(framebuffer,
                           raw_width,
                           raw_height,
                           portrait_width,
                           portrait_height,
                           content_left + std::max(0, (content_width - date_width) / 2),
                           cursor_y,
                           date_line,
                           date_role,
                           design::color::kBlack);
        cursor_y += LineHeight(date_role);
    }
    cursor_y += design::lock_screen::kDateDividerGap;

    FillPortraitRect(framebuffer,
                     raw_width,
                     raw_height,
                     portrait_width,
                     portrait_height,
                     {content_left, cursor_y, content_width, design::lock_screen::kDividerHeight},
                     design::color::kGray3);
    cursor_y += design::lock_screen::kDividerHeight + design::lock_screen::kDividerSectionGap;

    const int pending_count = std::max(0, state.pending_todo_count);
    std::string heading = "TO-DO";
    if (pending_count > 0) {
        heading += " - " + std::to_string(pending_count) + " PENDING";
    }
    const auto heading_role = design::TypographyRole::kLabelMediumBold;
    DrawTypographyText(framebuffer,
                       raw_width,
                       raw_height,
                       portrait_width,
                       portrait_height,
                       content_left,
                       cursor_y,
                       heading,
                       heading_role,
                       design::color::kBlack);
    cursor_y += LineHeight(heading_role) + design::lock_screen::kHeadingRowGap;

    if (pending_count == 0) {
        const EmbeddedImageAsset* check_icon = project_assets::GetIcon(EmbeddedIconId::kCheck);
        const auto body_role = design::TypographyRole::kBody;
        const std::string message = "All caught up";
        int text_x = content_left;
        int text_y = cursor_y;
        if (check_icon != nullptr) {
            DrawPortraitMonoAsset(framebuffer,
                                  raw_width,
                                  raw_height,
                                  portrait_width,
                                  portrait_height,
                                  content_left,
                                  cursor_y,
                                  check_icon,
                                  design::color::kBlack);
            text_x = content_left + static_cast<int>(check_icon->width) +
                    design::lock_screen::kCheckboxTextGap;
            text_y = cursor_y +
                    std::max(0, (static_cast<int>(check_icon->height) - LineHeight(body_role)) / 2);
        }
        DrawTypographyText(framebuffer,
                           raw_width,
                           raw_height,
                           portrait_width,
                           portrait_height,
                           text_x,
                           text_y,
                           message,
                           body_role,
                           design::color::kBlack);
    } else {
        const size_t row_count = std::min<size_t>(state.pending_todo_titles.size(), 3);
        for (size_t i = 0; i < row_count; ++i) {
            const int row_height = DrawTodoRow(framebuffer,
                                               raw_width,
                                               raw_height,
                                               portrait_width,
                                               portrait_height,
                                               content_left,
                                               cursor_y,
                                               content_width,
                                               state.pending_todo_titles[i]);
            cursor_y += row_height;
            if (i + 1 < row_count) {
                cursor_y += design::lock_screen::kRowGap;
            }
        }

        if (pending_count > static_cast<int>(row_count)) {
            cursor_y += design::lock_screen::kRowGap;
            const int more_count = pending_count - static_cast<int>(row_count);
            const std::string more_text =
                "+" + std::to_string(more_count) + " more pending";
            DrawTypographyText(framebuffer,
                               raw_width,
                               raw_height,
                               portrait_width,
                               portrait_height,
                               content_left,
                               cursor_y,
                               more_text,
                               design::TypographyRole::kLabelSmall,
                               design::color::kGray2);
        }
    }

    const EmbeddedImageAsset* lock_icon = project_assets::GetIcon(EmbeddedIconId::kLock);
    if (lock_icon != nullptr) {
        const int icon_size = design::lock_screen::kLockIconSize;
        const UiRect dest = {
            content_left + std::max(0, (content_width - icon_size) / 2),
            portrait_height - design::lock_screen::kLockIconBottomMargin - icon_size,
            icon_size,
            icon_size,
        };
        DrawScaledPortraitMonoAsset(framebuffer,
                                    raw_width,
                                    raw_height,
                                    portrait_width,
                                    portrait_height,
                                    dest,
                                    lock_icon,
                                    design::color::kBlack);
    }
}

}  // namespace epaper_ui
