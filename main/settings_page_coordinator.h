#ifndef SETTINGS_PAGE_COORDINATOR_H_
#define SETTINGS_PAGE_COORDINATOR_H_

#include "epaper_ui/settings_page.h"
#include "page_navigation/navigation_model.h"
#include "page_navigation/roving_focus.h"
#include "storage_service.h"
#include "wifi_service.h"

class SettingsPageCoordinator {
public:
    SettingsPageCoordinator();

    void Show();
    bool MoveFocus(int delta);
    bool SetFocusIndex(int index);
    bool IsRoleFocused(page_navigation::NavigationItemRole role) const;

    // Which item should anchor the top of the scrollable content area -- see
    // epaper_ui::SettingsPageResolveVisibleAnchor, which computes the value this should be set
    // to after any focus move (main/settings_page_runtime.cpp owns calling that, since it's the
    // one that knows the panel's actual portrait dimensions).
    void SetVisibleItemIndex(int index) { visible_item_index_ = index; }

    epaper_ui::SettingsPageState BuildState(const wifi_service::UiState& wifi_state,
                                            const storage_service::Snapshot& storage_snapshot,
                                            int archive_after_days) const;

    const page_navigation::NavigationModel& navigation_model() const { return navigation_model_; }
    const page_navigation::RovingFocus& focus() const { return focus_; }

private:
    static epaper_ui::ToggleVisualState BuildToggleState(bool enabled, bool focused);

    page_navigation::NavigationModel navigation_model_ =
        page_navigation::BuildSettingsPageNavigationModel();
    page_navigation::RovingFocus focus_{navigation_model_.item_count, 0};
    int visible_item_index_ = 0;
};

#endif  // SETTINGS_PAGE_COORDINATOR_H_
