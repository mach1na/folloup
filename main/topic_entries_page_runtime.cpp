#include "topic_entries_page_runtime.h"

#include <atomic>
#include <mutex>
#include <vector>

#include "esp_log.h"
#include "recording_archive_service.h"
#include "shared_page_interactions.h"
#include "topic_entries_page_coordinator.h"
#include "ui_refresh_runtime.h"

namespace topic_entries_page_runtime {
namespace {

constexpr const char* kTag = "TopicEntriesPageRuntime";
constexpr page_navigation::NavigationItemSection kGroupSection =
    page_navigation::NavigationItemSection::kTopicEntriesPageTimelineGroups;

std::mutex s_mutex;
TopicEntriesPageCoordinator s_coordinator = {};
std::atomic<bool> s_pending_back{false};
std::string s_pending_view_details_id;
bool s_pending_summary = false;

epaper_ui::TopicEntriesPageState BuildStateLocked()
{
    return s_coordinator.BuildState();
}

footer_runtime::ProjectionState BuildFooterProjectionStateLocked()
{
    return shared_page_interactions::BuildTimelineFooterProjectionState(s_coordinator,
                                                                        kGroupSection);
}

bool FooterProjectionChangedForFocusIndexes(int old_focus_index, int new_focus_index)
{
    return shared_page_interactions::TimelineFooterProjectionChanged(
        s_coordinator, old_focus_index, new_focus_index, kGroupSection);
}

}  // namespace

esp_err_t UpdateDisplayState()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return display_service::SetTopicEntriesPageState(BuildStateLocked());
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
    return ui_refresh_runtime::Schedule(ui_refresh_runtime::SurfaceKey::kTopicEntriesPage,
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
        result = topic_entries_page_interactions::HandleMoveFocus(s_coordinator, delta);
        if (!result.handled) {
            return result;
        }
        new_focus_index = s_coordinator.focus().index();
    }
    result.sync_footer_projection =
        FooterProjectionChangedForFocusIndexes(old_focus_index, new_focus_index);
    return result;
}

topic_entries_page_interactions::ActivateResult ActivateFocusedItem()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return topic_entries_page_interactions::HandlePrimaryActivate(s_coordinator);
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
        shared_page_interactions::FooterRoleForFooterItem(item);
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
        s_coordinator.ExitItemList();
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
        projection = BuildFooterProjectionStateLocked();
    }
    footer_runtime::SetProjectionState(projection);
}

void QueueShow(const std::string& topic_id, const std::string& topic_name)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    s_coordinator.QueueShow(topic_id, topic_name);
}

esp_err_t SyncFromArchive(bool request_refresh_if_active)
{
    std::vector<recording_archive_service::RecordingEntry> entries =
        recording_archive_service::ListRecordings();
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_coordinator.Show(entries);
    }
    const esp_err_t err =
        request_refresh_if_active
            ? UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode::kPartial)
            : UpdateDisplayState();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Topic entries sync failed: %s", esp_err_to_name(err));
    }
    return err;
}

bool ExitActiveControl()
{
    bool exited = false;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        exited = s_coordinator.ExitItemList();
    }
    if (exited) {
        (void)UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode::kPartial);
    }
    return exited;
}

void RequestBack()
{
    s_pending_back.store(true, std::memory_order_relaxed);
}

bool ConsumePendingBack()
{
    return s_pending_back.exchange(false, std::memory_order_relaxed);
}

void RequestViewDetails()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    const TopicEntriesPageCoordinator::TimelineEntry* entry = s_coordinator.SelectedEntry();
    if (entry != nullptr) {
        s_pending_view_details_id = entry->recording_id;
    }
}

std::string ConsumePendingViewDetails()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    std::string id;
    id.swap(s_pending_view_details_id);
    return id;
}

void RequestShowSummary()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    s_pending_summary = true;
}

PendingTopicSummary ConsumePendingShowSummary()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    PendingTopicSummary pending = {};
    if (!s_pending_summary) {
        return pending;
    }
    s_pending_summary = false;
    pending.valid = true;
    pending.topic_id = s_coordinator.topic_id();
    pending.topic_name = s_coordinator.topic_name();
    return pending;
}

}  // namespace topic_entries_page_runtime
