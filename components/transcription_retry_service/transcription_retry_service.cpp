#include "transcription_retry_service.h"

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gemini_service.h"
#include "recording_archive_service.h"
#include "recording_service.h"
#include "transcription_service.h"

namespace transcription_retry_service {
namespace {

constexpr const char* kTag = "TranscribeRetry";
// Per-item poll timeout: gemini_service's own HTTP transcription timeout is 30s
// (kTranscribeTimeoutMs); this leaves margin for task scheduling before giving up on one item.
constexpr int64_t kItemTimeoutUs = 40 * 1000 * 1000;
constexpr int64_t kPollIntervalMs = 250;

std::mutex s_mutex;
Snapshot s_snapshot = {};
EventHandler s_event_handler = nullptr;
void* s_event_context = nullptr;
std::atomic<bool> s_batch_in_flight{false};
std::atomic<bool> s_initialized{false};

void NotifyLocked()
{
    EventHandler handler = s_event_handler;
    void* context = s_event_context;
    if (handler == nullptr) {
        return;
    }
    const Event event = {.snapshot = s_snapshot};
    handler(event, context);
}

// Retries a single archived recording: load its clip off SD, submit it for transcription, and
// poll transcription_service for completion. Deliberately does not go through
// recording_session_service::BeginArchivedTranscription -- that shares a single-slot phase
// machine with the live recording UI flow, and a background batch running alongside a user
// press-and-hold would corrupt it. This function only touches recording_archive_service and
// transcription_service directly, using transcription_service's own request_in_flight guard as
// the sole serialization point.
bool RetryOne(const std::string& recording_id)
{
    if (transcription_service::GetSnapshot().request_in_flight) {
        // Something else already holds the single transcription slot (most likely a live
        // recording session). Treat this as the note's one automatic attempt failing; the
        // manual "Transcribe" button remains available afterward.
        ESP_LOGW(kTag, "Skipping retry for %s: transcription service busy", recording_id.c_str());
        recording_archive_service::ClearPendingTranscription(recording_id);
        return false;
    }

    recording_service::RecordedClipPtr clip = recording_archive_service::LoadClip(recording_id);
    if (!clip || clip->empty()) {
        ESP_LOGW(kTag, "Skipping retry for %s: clip load failed", recording_id.c_str());
        recording_archive_service::ClearPendingTranscription(recording_id);
        return false;
    }

    if (!transcription_service::BeginTranscription(clip)) {
        ESP_LOGW(kTag, "Skipping retry for %s: BeginTranscription refused", recording_id.c_str());
        recording_archive_service::ClearPendingTranscription(recording_id);
        return false;
    }

    const int64_t deadline_us = esp_timer_get_time() + kItemTimeoutUs;
    while (transcription_service::GetSnapshot().request_in_flight) {
        if (esp_timer_get_time() > deadline_us) {
            ESP_LOGW(kTag, "Retry for %s timed out waiting for transcription", recording_id.c_str());
            recording_archive_service::ClearPendingTranscription(recording_id);
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(kPollIntervalMs));
    }

    const transcription_service::Snapshot result = transcription_service::GetSnapshot();
    if (!result.last_error_code.empty() || result.last_transcript.empty()) {
        ESP_LOGW(kTag, "Retry for %s failed: error=%s", recording_id.c_str(),
                 result.last_error_code.empty() ? "<empty transcript>" : result.last_error_code.c_str());
        recording_archive_service::ClearPendingTranscription(recording_id);
        return false;
    }

    const recording_archive_service::SaveResult save_result =
        recording_archive_service::SaveTranscript(recording_id, result.last_transcript);
    if (!save_result.transcript_saved) {
        ESP_LOGW(kTag, "Retry for %s: transcript save failed", recording_id.c_str());
        recording_archive_service::ClearPendingTranscription(recording_id);
        return false;
    }

    ESP_LOGI(kTag, "Retry succeeded for %s", recording_id.c_str());
    return true;  // SaveTranscript already clears pending_transcription on success.
}

// Runs on gemini_service's shared worker task (see gemini_service::RunOnWorker). Since every
// Gemini-backed job is now serialized through that one task, this batch can never actually run
// concurrently with a live user transcription -- RetryOne's request_in_flight check above is a
// belt-and-suspenders guard, not the sole protection it used to be.
void RunTranscriptionRetryJob()
{
    esp_err_t status = ESP_OK;
    const std::vector<recording_archive_service::RecordingEntry> entries =
        recording_archive_service::ListRecordings(&status);

    int attempted = 0;
    int succeeded = 0;
    int failed = 0;
    if (status == ESP_OK) {
        for (const auto& entry : entries) {
            if (!entry.metadata.pending_transcription) {
                continue;
            }
            attempted++;
            if (RetryOne(entry.recording_id)) {
                succeeded++;
            } else {
                failed++;
            }
        }
    } else {
        ESP_LOGW(kTag, "Retry batch aborted: archive listing failed (%s)", esp_err_to_name(status));
    }

    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_snapshot.batch_in_flight = false;
        s_snapshot.last_batch_attempted = attempted;
        s_snapshot.last_batch_succeeded = succeeded;
        s_snapshot.last_batch_failed = failed;
        s_snapshot.last_batch_generation++;
        NotifyLocked();
    }

    ESP_LOGI(kTag, "Retry batch complete: attempted=%d succeeded=%d failed=%d", attempted, succeeded,
             failed);
    s_batch_in_flight.store(false, std::memory_order_release);
}

}  // namespace

esp_err_t Init()
{
    if (s_initialized.exchange(true)) {
        return ESP_OK;
    }
    std::lock_guard<std::mutex> lock(s_mutex);
    s_snapshot.initialized = true;
    return ESP_OK;
}

void SetEventHandler(EventHandler handler, void* context)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    s_event_handler = handler;
    s_event_context = context;
}

Snapshot GetSnapshot()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_snapshot;
}

bool RetryPending()
{
    if (Init() != ESP_OK) {
        return false;
    }
    if (recording_archive_service::GetSnapshot().pending_transcription_count == 0) {
        return false;
    }

    bool expected = false;
    if (!s_batch_in_flight.compare_exchange_strong(expected, true)) {
        return false;  // a batch is already running
    }

    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_snapshot.batch_in_flight = true;
        NotifyLocked();
    }

    gemini_service::RunOnWorker(&RunTranscriptionRetryJob);
    return true;
}

}  // namespace transcription_retry_service
