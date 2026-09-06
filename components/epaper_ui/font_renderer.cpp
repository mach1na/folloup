#include "epaper_ui/font_renderer.h"

namespace epaper_ui {

int MeasureText(design::TypographyRole role, std::string_view text)
{
    using font_renderer_detail::FontForRole;
    using font_renderer_detail::ScaleMetric;

    const font_renderer_detail::FontSelection selection = FontForRole(role);
    return ScaleMetric(epaper_font::MeasureText(*selection.font, text, selection.tracking),
                       selection);
}

int LineHeight(design::TypographyRole role)
{
    using font_renderer_detail::FontForRole;
    using font_renderer_detail::ScaleMetric;

    const font_renderer_detail::FontSelection selection = FontForRole(role);
    if (selection.line_height_override > 0) {
        return selection.line_height_override;
    }
    return ScaleMetric(selection.font->line_height, selection);
}

}  // namespace epaper_ui
