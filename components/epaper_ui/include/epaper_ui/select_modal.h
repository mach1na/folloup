#ifndef EPAPER_UI_SELECT_MODAL_H_
#define EPAPER_UI_SELECT_MODAL_H_

#include <string>
#include <vector>

#include "epaper_ui/overlay_geometry.h"

namespace epaper_ui {

struct SelectModalItemState {
    std::string label_text = {};
    // Only meaningful when the owning SelectModalState::multi_select is true. Renders a
    // checkmark (SelectItemState::checked already supports this) and is toggled in place by a
    // click rather than submitting the modal.
    bool checked = false;
    // Only meaningful when multi_select is true: a click on this item submits the modal (with
    // the current checked state of every item) instead of toggling. Ignored otherwise -- a
    // single-select modal always submits on click, exactly as before.
    bool is_submit = false;
};

struct SelectModalState {
    bool visible = false;
    std::string title_text = {};
    int selected_index = 0;
    std::vector<SelectModalItemState> items = {};
    // When true, a click on a non-is_submit item toggles its `checked` flag and keeps the modal
    // open, instead of the normal single-select submit-and-close. Existing single-select callers
    // leave this false and see no behavior change.
    bool multi_select = false;
};

UiRect SelectModalPanelBounds(int portrait_width,
                              int portrait_height,
                              const SelectModalState& state);
UiRect SelectModalItemBounds(int portrait_width,
                             int portrait_height,
                             const SelectModalState& state,
                             int index);
int HitTestSelectModalItem(int portrait_width,
                           int portrait_height,
                           const SelectModalState& state,
                           int x,
                           int y,
                           bool* hit);
void DrawSelectModal(uint8_t* framebuffer,
                     int raw_width,
                     int raw_height,
                     int portrait_width,
                     int portrait_height,
                     const SelectModalState& state);

}  // namespace epaper_ui

#endif  // EPAPER_UI_SELECT_MODAL_H_
