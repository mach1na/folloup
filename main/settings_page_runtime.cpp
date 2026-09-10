#include "settings_page_runtime.h"

#include <climits>
#include <cstddef>
#include <mutex>

#include "esp_log.h"
#include "overlay_runtime.h"
#include "page_navigation/navigation_model.h"
#include "page_navigation/page_focus_projection.h"
#include "recording_archive_service.h"
#include "settings_page_interactions.h"
#include "settings_page_coordinator.h"
#include "storage_service.h"
#include "ui_refresh_runtime.h"
#include "wifi_service.h"

namespace settings_page_runtime {
namespace {

constexpr const char* kTag = "SettingsPageRuntime";

// Fixed choices offered by the "Archive todos after" picker, mirroring the Time page's
// timezone SelectModal pattern rather than free-form numeric entry.
struct ArchiveAfterOption {
    const char* label;
    int days;
};
constexpr ArchiveAfterOption kArchiveAfterOptions[] = {
    {"7 days", 7}, {"14 days", 14}, {"30 days", 30},
    {"60 days", 60}, {"90 days", 90}, {"Never", 0},
};

std::mutex s_mutex;
SettingsPageCoordinator s_coordinator = {};
bool s_archive_after_modal_active = false;
int32_t s_interaction_generation = 1;

void AdvanceInteractionGenerationLocked()
{
    if (s_interaction_generation == INT32_MAX) {
        s_interaction_generation = 1;
    } else {
        ++s_interaction_generation;
    }
}

footer_runtime::FooterFocusItem FooterItemForSelectedIndex(int selected_index)
{
    switch (selected_index) {
        case 1:
            return footer_runtime::FooterFocusItem::kSettings;
        case 2:
            return footer_runtime::FooterFocusItem::kWifi;
        case 3:
            return footer_runtime::FooterFocusItem::kTime;
        case 0:
            return footer_runtime::FooterFocusItem::kHome;
        case 4:
            return footer_runtime::FooterFocusItem::kSticky;
        default:
            return footer_runtime::FooterFocusItem::kNone;
    }
}

page_navigation::NavigationItemRole FooterRoleForFooterItem(footer_runtime::FooterFocusItem item)
{
    switch (item) {
        case footer_runtime::FooterFocusItem::kSettings:
            return page_navigation::NavigationItemRole::kFooterSettings;
        case footer_runtime::FooterFocusItem::kWifi:
            return page_navigation::NavigationItemRole::kFooterWifi;
        case footer_runtime::FooterFocusItem::kHome:
            return page_navigation::NavigationItemRole::kFooterHome;
        case footer_runtime::FooterFocusItem::kTime:
            return page_navigation::NavigationItemRole::kFooterTime;
        case footer_runtime::FooterFocusItem::kSticky:
            return page_navigation::NavigationItemRole::kFooterSticky;
        case footer_runtime::FooterFocusItem::kNone:
        case footer_runtime::FooterFocusItem::kFolder:
        case footer_runtime::FooterFocusItem::kMic:
        default:
            return page_navigation::NavigationItemRole::kUnknown;
    }
}

footer_runtime::ProjectionState BuildFooterProjectionStateLocked()
{
    const page_navigation::PageFocusProjection projection =
        page_navigation::ProjectPageFocus(s_coordinator.navigation_model(),
                                          page_navigation::NavigationItemSection::kSettingsPageMenu,
                                          s_coordinator.focus().index(),
                                          -1,
                                          -1);
    footer_runtime::ProjectionState state = {};
    state.focused_item = FooterItemForSelectedIndex(projection.footer_selected_index);
    return state;
}

bool FooterProjectionChangedForFocusIndexes(int old_focus_index, int new_focus_index)
{
    const page_navigation::PageFocusProjection old_projection =
        page_navigation::ProjectPageFocus(s_coordinator.navigation_model(),
                                          page_navigation::NavigationItemSection::kSettingsPageMenu,
                                          old_focus_index,
                                          -1,
                                          -1);
    const page_navigation::PageFocusProjection new_projection =
        page_navigation::ProjectPageFocus(s_coordinator.navigation_model(),
                                          page_navigation::NavigationItemSection::kSettingsPageMenu,
                                          new_focus_index,
                                          -1,
                                          -1);
    return FooterItemForSelectedIndex(old_projection.footer_selected_index) !=
           FooterItemForSelectedIndex(new_projection.footer_selected_index);
}

epaper_ui::SettingsPageState BuildStateLocked()
{
    return s_coordinator.BuildState(wifi_service::GetUiState(), storage_service::GetSnapshot(),
                                    recording_archive_service::GetArchiveAfterDays());
}

// Recomputes which item should anchor the top of the scrollable content area for whichever item
// is now focused, and stores it back on the coordinator. Must run after every focus move, still
// under s_mutex, using the just-rebuilt state (so it sees the new focus) -- the *next* BuildState
// call (for the actual repaint) then picks up the resolved anchor. Lives here rather than on the
// coordinator because it's the one thing about scrolling that needs the panel's actual portrait
// dimensions, which the coordinator itself doesn't know.
void SyncVisibleItemIndexLocked(const epaper_ui::SettingsPageState& state_after_move)
{
    s_coordinator.SetVisibleItemIndex(epaper_ui::SettingsPageResolveVisibleAnchor(
        display_service::PortraitWidth(), display_service::PortraitHeight(), state_after_move));
}

}  // namespace

esp_err_t UpdateDisplayState()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return display_service::SetSettingsPageState(BuildStateLocked());
}

esp_err_t UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode refresh_mode)
{
    return UpdateDisplayStateAndRequestRefresh(display_service::RefreshRequest{
        .refresh_mode = refresh_mode,
    });
}

esp_err_t UpdateDisplayStateAndRequestRefresh(
    const display_service::RefreshRequest& refresh_request)
{
    return ui_refresh_runtime::Schedule(
        ui_refresh_runtime::SurfaceKey::kSettingsPage, &UpdateDisplayState, refresh_request);
}

page_actions::FocusMoveOutcome MoveFocus(int delta)
{
    page_actions::FocusMoveOutcome result = {};
    int old_focus_index = -1;
    int new_focus_index = -1;
    epaper_ui::SettingsPageState old_state = {};
    epaper_ui::SettingsPageState new_state = {};
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        old_focus_index = s_coordinator.focus().index();
        old_state = BuildStateLocked();
        result = settings_page_interactions::HandleMoveFocus(s_coordinator, delta);
        if (!result.handled) {
            return result;
        }
        new_focus_index = s_coordinator.focus().index();
        new_state = BuildStateLocked();
        SyncVisibleItemIndexLocked(new_state);
    }

    result.sync_footer_projection =
        FooterProjectionChangedForFocusIndexes(old_focus_index, new_focus_index);    return result;
}

settings_page_interactions::ActivateResult ActivateFocusedItem()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return settings_page_interactions::HandlePrimaryActivate(s_coordinator);
}

footer_runtime::ProjectionState BuildFooterProjectionState()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return BuildFooterProjectionStateLocked();
}

page_actions::FocusUpdateOutcome FocusFooterItem(footer_runtime::FooterFocusItem item)
{
    page_actions::FocusUpdateOutcome result = {};
    const page_navigation::NavigationItemRole role =
        FooterRoleForFooterItem(item);
    if (role == page_navigation::NavigationItemRole::kUnknown) {
        return result;
    }

    int old_focus_index = -1;
    int new_focus_index = -1;
    epaper_ui::SettingsPageState old_state = {};
    epaper_ui::SettingsPageState new_state = {};
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        const int focus_index = s_coordinator.navigation_model().IndexOfRole(role);
        if (focus_index < 0) {
            return result;
        }
        old_focus_index = s_coordinator.focus().index();
        old_state = BuildStateLocked();
        if (!s_coordinator.SetFocusIndex(focus_index)) {
            return result;
        }
        new_focus_index = s_coordinator.focus().index();
        new_state = BuildStateLocked();
        SyncVisibleItemIndexLocked(new_state);
    }

    result.handled = true;
    result.apply_page_state = true;
    result.sync_footer_projection =
        FooterProjectionChangedForFocusIndexes(old_focus_index, new_focus_index);    return result;
}

void ResetFocus()
{
    footer_runtime::ProjectionState projection = {};
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_coordinator.Show();
        AdvanceInteractionGenerationLocked();
        projection = BuildFooterProjectionStateLocked();
    }
    footer_runtime::SetProjectionState(projection);
}

esp_err_t ShowArchiveAfterModal()
{
    epaper_ui::SelectModalState state = {};
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        state.visible = true;
        state.title_text = "Archive todos after";
        const int current_days = recording_archive_service::GetArchiveAfterDays();
        state.selected_index = 0;
        for (size_t index = 0; index < std::size(kArchiveAfterOptions); ++index) {
            state.items.push_back({.label_text = kArchiveAfterOptions[index].label});
            if (kArchiveAfterOptions[index].days == current_days) {
                state.selected_index = static_cast<int>(index);
            }
        }
        s_archive_after_modal_active = true;
    }
    return overlay_runtime::ShowSelectModal(state);
}

bool HandleSelectModalSubmit(int selected_index)
{
    bool was_active = false;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        was_active = s_archive_after_modal_active;
        s_archive_after_modal_active = false;
    }
    if (!was_active) {
        return false;
    }
    if (selected_index >= 0 && selected_index < static_cast<int>(std::size(kArchiveAfterOptions))) {
        (void)recording_archive_service::SetArchiveAfterDays(
            kArchiveAfterOptions[selected_index].days);
    }
    (void)UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode::kPartial);
    return true;
}

void ClearPendingSelectModal()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    s_archive_after_modal_active = false;
}

}  // namespace settings_page_runtime
