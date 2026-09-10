#include "lock_screen_runtime.h"

#include <algorithm>
#include <atomic>
#include <climits>
#include <ctime>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "device_sleep_service.h"
#include "display_service.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "followup_task_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "recording_archive_service.h"
#include "status_bar_runtime.h"
#include "timeline_format.h"
#include "ui_refresh_runtime.h"

namespace lock_screen_runtime {
namespace {

constexpr const char* kTag = "LockScreenRuntime";
constexpr time_t kMinValidEpoch = 1600000000;
constexpr uint64_t kClockPollPeriodUs = 1000 * 1000;
constexpr size_t kMaxPendingTodoTitles = 5;
// Runs on its own dedicated task rather than whatever caller triggered RefreshTodoSummary()
// (an archive-changed event can fire from very different stack budgets -- e.g. the 4096-word
// input_callbacks dispatcher used by the recording-save flow, which a full ListRecordings()
// scan plus this file's own filtering/sorting on top overflowed).
constexpr uint32_t kTodoSummaryTaskStackWords = 6144;

std::mutex s_mutex;
bool s_initialized = false;
bool s_active = false;
display_service::ScreenId s_restore_screen = display_service::ScreenId::kHome;
esp_timer_handle_t s_clock_timer = nullptr;
uint32_t s_last_minute_key = UINT_MAX;
std::atomic<bool> s_todo_summary_refresh_in_flight = false;
epaper_ui::LockScreenState s_state = {};

uint32_t BuildMinuteKey(time_t now)
{
    if (now < kMinValidEpoch) {
        return UINT_MAX;
    }
    return static_cast<uint32_t>(now / 60);
}

// Rebuilds only the weekday/date fields of s_state, preserving whatever pending-todo
// summary is already cached there (that's refreshed independently, by RefreshTodoSummary).
bool RebuildDateStateLocked(bool force)
{
    const time_t now = time(nullptr);
    const uint32_t minute_key = BuildMinuteKey(now);
    if (!force && minute_key == s_last_minute_key) {
        return false;
    }

    std::string weekday_text;
    std::string date_text;
    if (now >= kMinValidEpoch) {
        std::tm local_tm = {};
        localtime_r(&now, &local_tm);

        char weekday_buf[16] = {};
        char month_buf[8] = {};
        strftime(weekday_buf, sizeof(weekday_buf), "%A", &local_tm);
        strftime(month_buf, sizeof(month_buf), "%b", &local_tm);

        weekday_text = weekday_buf;
        date_text = std::string(month_buf) + " " + std::to_string(local_tm.tm_mday) + ", " +
                   std::to_string(local_tm.tm_year + 1900);
    }

    const bool changed =
        force || weekday_text != s_state.weekday_text || date_text != s_state.date_text;
    if (!changed) {
        s_last_minute_key = minute_key;
        return false;
    }

    s_state.weekday_text = std::move(weekday_text);
    s_state.date_text = std::move(date_text);
    s_last_minute_key = minute_key;
    return true;
}

int64_t EntryTimestamp(const recording_archive_service::RecordingEntry& entry)
{
    return entry.metadata.created_unix_seconds > 0 ? entry.metadata.created_unix_seconds
                                                   : entry.modified_unix_seconds;
}

bool IsPendingTodo(const recording_archive_service::RecordingEntry& entry)
{
    return entry.metadata.tag == recording_archive_service::RecordingTag::kTask &&
           !entry.metadata.completed;
}

std::string TodoDisplayText(const recording_archive_service::RecordingEntry& entry)
{
    const std::string trimmed = timeline_format::TrimTranscript(entry.transcript_text);
    return entry.metadata.has_transcript && !trimmed.empty() ? trimmed : "Audio only todo.";
}

esp_err_t UpdateDisplayState()
{
    epaper_ui::LockScreenState state = {};
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (!s_initialized) {
            return ESP_ERR_INVALID_STATE;
        }
        state = s_state;
    }

    return display_service::SetLockScreenState(state);
}

esp_err_t PushState(epaper_ui::LockScreenState state,
                    bool active,
                    bool request_refresh_if_active)
{
    ESP_RETURN_ON_ERROR(display_service::SetLockScreenState(state),
                        kTag,
                        "set lock screen state failed");
    if (active && request_refresh_if_active) {
        return ui_refresh_runtime::Schedule(ui_refresh_runtime::SurfaceKey::kLockScreen,
                                            &UpdateDisplayState,
                                            display_service::RefreshMode::kPartial);
    }
    return ESP_OK;
}

void TodoSummaryWorkerTask(void*)
{
    esp_err_t list_status = ESP_OK;
    const std::vector<recording_archive_service::RecordingEntry> entries =
        recording_archive_service::ListRecordings(&list_status);
    if (list_status != ESP_OK) {
        ESP_LOGW(kTag, "Todo summary refresh: list recordings failed: %s",
                 esp_err_to_name(list_status));
    } else {
        // Follow-up flagged todos surface first (newest first within each group), then rest.
        std::vector<const recording_archive_service::RecordingEntry*> follow_up_first;
        std::vector<const recording_archive_service::RecordingEntry*> rest;
        for (const auto& entry : entries) {
            if (!IsPendingTodo(entry)) {
                continue;
            }
            (entry.metadata.follow_up ? follow_up_first : rest).push_back(&entry);
        }
        const auto by_recency = [](const auto* a, const auto* b) {
            return EntryTimestamp(*a) > EntryTimestamp(*b);
        };
        std::sort(follow_up_first.begin(), follow_up_first.end(), by_recency);
        std::sort(rest.begin(), rest.end(), by_recency);

        std::vector<std::string> titles;
        titles.reserve(kMaxPendingTodoTitles);
        for (auto* group : {&follow_up_first, &rest}) {
            for (const auto* entry : *group) {
                if (titles.size() >= kMaxPendingTodoTitles) {
                    break;
                }
                titles.push_back(TodoDisplayText(*entry));
            }
        }
        const int pending_count = static_cast<int>(follow_up_first.size() + rest.size());

        epaper_ui::LockScreenState state = {};
        bool active = false;
        bool push = false;
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            if (s_initialized) {
                s_state.pending_todo_titles = std::move(titles);
                s_state.pending_todo_count = pending_count;
                active = s_active;
                state = s_state;
                push = true;
            }
        }
        if (push) {
            const esp_err_t push_err = PushState(state, active, true);
            if (push_err != ESP_OK) {
                ESP_LOGW(kTag, "Todo summary push failed: %s", esp_err_to_name(push_err));
            }
        }
    }

    s_todo_summary_refresh_in_flight.store(false, std::memory_order_relaxed);
    vTaskDelete(nullptr);
}

void OnClockTimer(void*)
{
    (void)SyncClockState(false);
}

esp_err_t ShowImpl(bool for_shutdown)
{
    epaper_ui::LockScreenState state = {};
    bool already_active = false;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (!s_initialized) {
            return ESP_ERR_INVALID_STATE;
        }
        already_active = s_active;
        if (!s_active) {
            const display_service::ScreenId current_screen = display_service::GetCurrentScreen();
            if (current_screen != display_service::ScreenId::kLockScreen) {
                s_restore_screen = current_screen;
            }
        }
        (void)RebuildDateStateLocked(true);
        s_active = true;
        state = s_state;
    }
    // Set on the local copy only, never persisted into s_state -- this is a one-shot
    // terminal paint (the device is about to lose power), not lock-screen state that
    // should survive into a later, normal repaint.
    state.for_shutdown = for_shutdown;

    ESP_RETURN_ON_ERROR(PushState(state, false, false), kTag, "push lock state failed");
    const esp_err_t status_bar_err = status_bar_runtime::UpdateDisplayState();
    if (status_bar_err != ESP_OK && status_bar_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Status row update before lock screen show failed: %s",
                 esp_err_to_name(status_bar_err));
    }

    // Shutdown must actually finish painting before power cuts, and must work even if
    // the panel is currently asleep -- WakeDisplayToScreen blocks until the panel
    // hardware is done and repaints unconditionally either way. A normal Show() instead
    // queues through the async display command path like every other screen
    // transition, since nothing here needs to block the caller.
    const esp_err_t err =
        for_shutdown
            ? display_service::WakeDisplayToScreen(display_service::ScreenId::kLockScreen)
            : display_service::SetCurrentScreen(display_service::ScreenId::kLockScreen,
                                                display_service::RefreshMode::kFull,
                                                "lock_screen_show");
    if (err != ESP_OK) {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_active = already_active;
        return err;
    }

    if (!for_shutdown) {
        // Locking is meant to put the display to sleep immediately rather than sit lit
        // until the normal inactivity timer elapses on its own. Light sleep (which is what
        // actually drops Wi-Fi) still follows its usual configured timeout from this point
        // -- it isn't forced -- since it costs a real Wi-Fi reassociation and a forced SD
        // remount on wake, which a quick lock/unlock cycle shouldn't have to pay every time.
        // No-op if already asleep or auto-sleep is disabled/misconfigured.
        (void)device_sleep_service::ForceDisplaySleep();
    }
    return err;
}

esp_err_t HideImpl(bool waking)
{
    display_service::ScreenId restore_screen = display_service::ScreenId::kHome;
    bool was_active = false;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (!s_initialized) {
            return ESP_ERR_INVALID_STATE;
        }
        if (!s_active) {
            return ESP_OK;
        }
        was_active = s_active;
        s_active = false;
        restore_screen = s_restore_screen;
    }

    const esp_err_t status_bar_err = status_bar_runtime::UpdateDisplayState();
    if (status_bar_err != ESP_OK && status_bar_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Status bar update before lock screen hide failed: %s",
                 esp_err_to_name(status_bar_err));
    }
    // Waking directly to the restore screen must go through WakeDisplayToScreen, not
    // SetCurrentScreen: the panel is still asleep at this point, and SetCurrentScreen's
    // change would just queue behind the separate wake call with no ordering guarantee
    // between the two, risking a redundant full refresh of the lock screen first.
    const esp_err_t err =
        waking ? display_service::WakeDisplayToScreen(restore_screen)
              : display_service::SetCurrentScreen(restore_screen,
                                                   display_service::RefreshMode::kFull,
                                                   "lock_screen_hide");
    if (err != ESP_OK) {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_active = was_active;
    }
    return err;
}

}  // namespace

esp_err_t Init()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_initialized) {
        return ESP_OK;
    }

    esp_timer_create_args_t timer_args = {};
    timer_args.callback = &OnClockTimer;
    timer_args.dispatch_method = ESP_TIMER_TASK;
    timer_args.name = "lock_screen";
    timer_args.skip_unhandled_events = true;

    ESP_RETURN_ON_ERROR(esp_timer_create(&timer_args, &s_clock_timer),
                        kTag,
                        "clock timer create failed");
    ESP_RETURN_ON_ERROR(esp_timer_start_periodic(s_clock_timer, kClockPollPeriodUs),
                        kTag,
                        "clock timer start failed");

    s_initialized = true;
    ESP_LOGI(kTag, "Lock screen runtime initialized");
    return ESP_OK;
}

bool IsActive()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_active;
}

esp_err_t Show()
{
    return ShowImpl(false);
}

esp_err_t ShowForShutdown()
{
    return ShowImpl(true);
}

esp_err_t Hide()
{
    return HideImpl(false);
}

esp_err_t HideWaking()
{
    return HideImpl(true);
}

esp_err_t Toggle()
{
    return IsActive() ? Hide() : Show();
}

esp_err_t RequestRefresh(display_service::RefreshMode refresh_mode)
{
    return ui_refresh_runtime::RequestRefresh(ui_refresh_runtime::SurfaceKey::kLockScreen,
                                              refresh_mode);
}

esp_err_t SyncClockState(bool request_refresh_if_active)
{
    epaper_ui::LockScreenState state = {};
    bool active = false;
    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (!s_initialized) {
            return ESP_ERR_INVALID_STATE;
        }
        active = s_active;
        if (!active && !request_refresh_if_active) {
            return ESP_OK;
        }
        changed = RebuildDateStateLocked(request_refresh_if_active);
        if (!changed) {
            return ESP_OK;
        }
        state = s_state;
    }

    return PushState(state, active, request_refresh_if_active);
}

esp_err_t RefreshTodoSummary()
{
    if (!s_todo_summary_refresh_in_flight.exchange(true, std::memory_order_relaxed)) {
        const BaseType_t created = xTaskCreatePinnedToCore(TodoSummaryWorkerTask,
                                                           "lockscr_todos",
                                                           kTodoSummaryTaskStackWords,
                                                           nullptr,
                                                           followup_task_config::kPriorityStorage,
                                                           nullptr,
                                                           followup_task_config::kSystemCore);
        if (created != pdPASS) {
            s_todo_summary_refresh_in_flight.store(false, std::memory_order_relaxed);
            ESP_LOGW(kTag, "Failed to start todo summary refresh task");
            return ESP_ERR_NO_MEM;
        }
    }
    // Else: a refresh is already in flight and will pick up current archive state on its own
    // -- not an error, just a no-op for this call.
    return ESP_OK;
}

}  // namespace lock_screen_runtime
