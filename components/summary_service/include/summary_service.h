#ifndef SUMMARY_SERVICE_H_
#define SUMMARY_SERVICE_H_

#include <cstdint>
#include <string>

#include "esp_err.h"

namespace summary_service {

enum class SummaryKind : uint8_t {
    kNone = 0,
    kNotes,
    kTodos,
    kTopic,
};

enum class RequestPhase : uint8_t {
    kIdle = 0,
    kStarted,
    kSucceeded,
    kFailed,
};

// Provenance of a cached summary: how many recordings fed it, how many had transcripts, and
// whether the input had to be truncated or chunked to fit the model budget.
struct CacheMetadata {
    int64_t generated_unix_seconds = 0;
    int source_item_count = 0;
    int transcript_item_count = 0;
    int missing_transcript_item_count = 0;
    bool truncated = false;
    bool chunked = false;
    int window_days = 0;
};

struct CacheEntrySnapshot {
    bool available = false;
    std::string text = {};
    CacheMetadata metadata = {};
};

struct RequestSnapshot {
    bool in_flight = false;
    SummaryKind kind = SummaryKind::kNone;
    // Only meaningful when kind == kTopic.
    std::string topic_id = {};
    RequestPhase phase = RequestPhase::kIdle;
    std::string status_message = {};
    std::string error_code = {};
    std::string error_message = {};
};

struct Snapshot {
    bool initialized = false;
    bool storage_available = false;
    CacheEntrySnapshot notes = {};
    CacheEntrySnapshot todos = {};
    // The last topic requested/completed via RequestTopicSummary -- one slot, matching the
    // service's existing "one thing at a time" design (the shared in-flight guard already
    // covers Notes/Todos/Topic alike).
    std::string topic_id = {};
    CacheEntrySnapshot topic = {};
    RequestSnapshot request = {};
    uint32_t request_generation = 0;
};

struct Event {
    Snapshot snapshot = {};
};

using EventHandler = void (*)(const Event& event, void* context);

// Creates the worker task + request queue and seeds the snapshot from any cached summaries on
// the SD card. Safe to call once at startup.
esp_err_t Init();
void SetEventHandler(EventHandler handler, void* context);
Snapshot GetSnapshot();

// Re-read the persisted summaries from SD into the snapshot (runs SD I/O on the caller's task).
bool RefreshCachedSummaries();
// Drop cached summaries after an SD format so a summary screen doesn't show stale results.
void ResetForFormat();
// Queue an async summary generation for Notes or Todos. Returns false if it can't be queued
// (not initialized, a request already in flight, or queue full). Progress is reported via events.
bool RequestSummary(SummaryKind kind);
// Queue an async summary generation for one topic (every recording tagged with topic_id, no time
// window). Same in-flight guard as RequestSummary -- only one summary request can run at a time.
bool RequestTopicSummary(const std::string& topic_id, const std::string& topic_name);

// Synchronous SD read of a topic's previously generated summary, so a topic's cached summary can
// show immediately on screen entry without waiting on Gemini. Returns {available = false} if no
// cache file exists yet for this topic. Pure read -- does not touch the shared snapshot; only a
// completed RequestTopicSummary does that (see Snapshot::topic).
CacheEntrySnapshot LoadTopicSummaryCache(const std::string& topic_id);

const char* SummaryKindName(SummaryKind kind);

}  // namespace summary_service

#endif  // SUMMARY_SERVICE_H_
