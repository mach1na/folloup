#include "topic_summary_page_runtime.h"

#include <atomic>
#include <climits>
#include <mutex>

#include "epaper_ui/topic_summary_page.h"
#include "esp_log.h"
#include "gemini_service.h"
#include "page_navigation/page_focus_projection.h"
#include "topic_summary_page_coordinator.h"
#include "ui_refresh_runtime.h"

namespace topic_summary_page_runtime {
namespace {

constexpr const char* kTag = "TopicSummaryPageRuntime";

std::mutex s_mutex;
TopicSummaryPageCoordinator s_coordinator = {};
summary_service::CacheEntrySnapshot s_cache = {};
std::atomic<bool> s_pending_back{false};
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

epaper_ui::TopicSummaryPageState BuildStateLocked()
{
    const bool gemini_ready = gemini_service::GetSnapshot().runtime.ready;
    return s_coordinator.BuildState(gemini_ready, s_cache);
}

footer_runtime::ProjectionState BuildFooterProjectionStateLocked()
{
    const page_navigation::PageFocusProjection projection = page_navigation::ProjectPageFocus(
        s_coordinator.navigation_model(),
        page_navigation::NavigationItemSection::kTopicSummaryPageControls,
        s_coordinator.focus().index(), -1, -1);
    footer_runtime::ProjectionState state = {};
    state.focused_item = FooterItemForSelectedIndex(projection.footer_selected_index);
    return state;
}

bool FooterProjectionChangedForFocusIndexes(int old_focus_index, int new_focus_index)
{
    const page_navigation::PageFocusProjection old_projection = page_navigation::ProjectPageFocus(
        s_coordinator.navigation_model(),
        page_navigation::NavigationItemSection::kTopicSummaryPageControls, old_focus_index, -1, -1);
    const page_navigation::PageFocusProjection new_projection = page_navigation::ProjectPageFocus(
        s_coordinator.navigation_model(),
        page_navigation::NavigationItemSection::kTopicSummaryPageControls, new_focus_index, -1, -1);
    return FooterItemForSelectedIndex(old_projection.footer_selected_index) !=
           FooterItemForSelectedIndex(new_projection.footer_selected_index);
}

}  // namespace

esp_err_t UpdateDisplayState()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return display_service::SetTopicSummaryPageState(BuildStateLocked());
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
    return ui_refresh_runtime::Schedule(ui_refresh_runtime::SurfaceKey::kTopicSummaryPage,
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
        result = topic_summary_page_interactions::HandleMoveFocus(s_coordinator, delta);
        if (!result.handled) {
            return result;
        }
        new_focus_index = s_coordinator.focus().index();
    }
    result.sync_footer_projection =
        FooterProjectionChangedForFocusIndexes(old_focus_index, new_focus_index);
    return result;
}

topic_summary_page_interactions::ActivateResult ActivateFocusedItem()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    const bool gemini_ready = gemini_service::GetSnapshot().runtime.ready;
    return topic_summary_page_interactions::HandlePrimaryActivate(s_coordinator, gemini_ready);
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
        s_coordinator.ExitScrollContainer();
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
        AdvanceInteractionGenerationLocked();
        projection = BuildFooterProjectionStateLocked();
    }
    footer_runtime::SetProjectionState(projection);
}

void QueueShow(const std::string& topic_id, const std::string& topic_name)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    s_coordinator.QueueShow(topic_id, topic_name);
}

esp_err_t SyncFromService(bool request_refresh_if_active)
{
    std::string topic_id;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_coordinator.Show();
        topic_id = s_coordinator.topic_id();
    }
    const summary_service::CacheEntrySnapshot cache = summary_service::LoadTopicSummaryCache(topic_id);
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        // Only adopt the load if this is still the topic being shown (a fast back-and-forth could
        // have moved on to a different topic while the SD read was in flight).
        if (s_coordinator.topic_id() == topic_id) {
            s_cache = cache;
        }
    }
    const esp_err_t err =
        request_refresh_if_active
            ? UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode::kPartial)
            : UpdateDisplayState();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Topic summary sync failed: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t OnSummarySnapshot(const summary_service::Snapshot& snapshot, bool request_refresh)
{
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_cache = snapshot.topic;
    }
    return request_refresh ? UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode::kPartial)
                           : UpdateDisplayState();
}

void EnterScroll()
{
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_coordinator.EnterScrollContainer();
    }
    (void)UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode::kPartial);
}

void RequestSummary()
{
    std::string topic_id;
    std::string topic_name;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        topic_id = s_coordinator.topic_id();
        topic_name = s_coordinator.topic_name();
    }
    (void)summary_service::RequestTopicSummary(topic_id, topic_name);
}

bool ExitActiveControl()
{
    bool exited = false;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        exited = s_coordinator.ExitScrollContainer();
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

std::string CurrentTopicId()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_coordinator.topic_id();
}

}  // namespace topic_summary_page_runtime
