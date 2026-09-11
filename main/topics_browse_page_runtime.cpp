#include "topics_browse_page_runtime.h"

#include <mutex>

#include "page_navigation/navigation_model.h"
#include "page_navigation/page_focus_projection.h"
#include "topic_service.h"
#include "topics_browse_page_coordinator.h"
#include "ui_refresh_runtime.h"

namespace topics_browse_page_runtime {
namespace {

std::mutex s_mutex;
TopicsBrowsePageCoordinator s_coordinator = {};
bool s_pending_entries = false;
std::string s_pending_topic_id;
std::string s_pending_topic_name;

int CurrentTopicCount()
{
    return static_cast<int>(topic_service::GetSnapshot().topics.size());
}

footer_runtime::FooterFocusItem FooterItemForSelectedIndex(int selected_index)
{
    return selected_index == 0 ? footer_runtime::FooterFocusItem::kHome
                                : footer_runtime::FooterFocusItem::kNone;
}

page_navigation::NavigationItemRole FooterRoleForFooterItem(footer_runtime::FooterFocusItem item)
{
    return item == footer_runtime::FooterFocusItem::kHome
               ? page_navigation::NavigationItemRole::kFooterHome
               : page_navigation::NavigationItemRole::kUnknown;
}

footer_runtime::ProjectionState BuildFooterProjectionStateLocked()
{
    const page_navigation::PageFocusProjection projection = page_navigation::ProjectPageFocus(
        s_coordinator.navigation_model(), page_navigation::NavigationItemSection::kNone,
        s_coordinator.focus().index(), -1, -1);
    footer_runtime::ProjectionState state = {};
    state.focused_item = FooterItemForSelectedIndex(projection.footer_selected_index);
    return state;
}

bool FooterProjectionChangedForFocusIndexes(int old_focus_index, int new_focus_index)
{
    const page_navigation::PageFocusProjection old_projection = page_navigation::ProjectPageFocus(
        s_coordinator.navigation_model(), page_navigation::NavigationItemSection::kNone,
        old_focus_index, -1, -1);
    const page_navigation::PageFocusProjection new_projection = page_navigation::ProjectPageFocus(
        s_coordinator.navigation_model(), page_navigation::NavigationItemSection::kNone,
        new_focus_index, -1, -1);
    return FooterItemForSelectedIndex(old_projection.footer_selected_index) !=
           FooterItemForSelectedIndex(new_projection.footer_selected_index);
}

epaper_ui::TopicsBrowsePageState BuildStateLocked()
{
    return s_coordinator.BuildState(topic_service::GetSnapshot());
}

}  // namespace

esp_err_t UpdateDisplayState()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return display_service::SetTopicsBrowsePageState(BuildStateLocked());
}

esp_err_t UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode refresh_mode)
{
    return UpdateDisplayStateAndRequestRefresh(
        display_service::RefreshRequest{.refresh_mode = refresh_mode});
}

esp_err_t UpdateDisplayStateAndRequestRefresh(
    const display_service::RefreshRequest& refresh_request)
{
    return ui_refresh_runtime::Schedule(ui_refresh_runtime::SurfaceKey::kTopicsBrowsePage,
                                        &UpdateDisplayState, refresh_request);
}

page_actions::FocusMoveOutcome MoveFocus(int delta)
{
    page_actions::FocusMoveOutcome result = {};
    int old_focus_index = -1;
    int new_focus_index = -1;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        old_focus_index = s_coordinator.focus().index();
        result = topics_browse_page_interactions::HandleMoveFocus(s_coordinator, delta);
        if (!result.handled) {
            return result;
        }
        new_focus_index = s_coordinator.focus().index();
    }

    result.sync_footer_projection =
        FooterProjectionChangedForFocusIndexes(old_focus_index, new_focus_index);
    return result;
}

topics_browse_page_interactions::ActivateResult ActivateFocusedItem()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return topics_browse_page_interactions::HandlePrimaryActivate(s_coordinator);
}

footer_runtime::ProjectionState BuildFooterProjectionState()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return BuildFooterProjectionStateLocked();
}

page_actions::FocusUpdateOutcome FocusFooterItem(footer_runtime::FooterFocusItem item)
{
    page_actions::FocusUpdateOutcome result = {};
    const page_navigation::NavigationItemRole role = FooterRoleForFooterItem(item);
    if (role == page_navigation::NavigationItemRole::kUnknown) {
        return result;
    }

    int old_focus_index = -1;
    int new_focus_index = -1;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        const int focus_index = s_coordinator.navigation_model().IndexOfRole(role);
        if (focus_index < 0) {
            return result;
        }
        old_focus_index = s_coordinator.focus().index();
        if (!s_coordinator.SetFocusIndex(focus_index)) {
            return result;
        }
        new_focus_index = s_coordinator.focus().index();
    }

    result.handled = true;
    result.apply_page_state = true;
    result.sync_footer_projection =
        FooterProjectionChangedForFocusIndexes(old_focus_index, new_focus_index);
    return result;
}

void ResetFocus()
{
    footer_runtime::ProjectionState projection = {};
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_coordinator.Show(CurrentTopicCount());
        projection = BuildFooterProjectionStateLocked();
    }
    footer_runtime::SetProjectionState(projection);
}

esp_err_t SyncFromTopicService(bool request_refresh_if_active)
{
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_coordinator.Sync(CurrentTopicCount());
    }
    const bool active =
        display_service::GetCurrentScreen() == display_service::ScreenId::kTopicsBrowse;
    if (request_refresh_if_active && active) {
        return UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode::kPartial);
    }
    return UpdateDisplayState();
}

void RequestShowEntriesForFocusedTopic()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    const int index = s_coordinator.FocusedTopicIndex();
    const topic_service::Snapshot snapshot = topic_service::GetSnapshot();
    if (index < 0 || index >= static_cast<int>(snapshot.topics.size())) {
        return;
    }
    s_pending_topic_id = snapshot.topics[static_cast<size_t>(index)].id;
    s_pending_topic_name = snapshot.topics[static_cast<size_t>(index)].name;
    s_pending_entries = true;
}

PendingTopicEntries ConsumePendingShowEntries()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    PendingTopicEntries pending = {};
    if (!s_pending_entries) {
        return pending;
    }
    s_pending_entries = false;
    pending.valid = true;
    pending.topic_id.swap(s_pending_topic_id);
    pending.topic_name.swap(s_pending_topic_name);
    return pending;
}

}  // namespace topics_browse_page_runtime
