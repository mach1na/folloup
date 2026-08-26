#ifndef TRANSCRIPTION_RETRY_SERVICE_H_
#define TRANSCRIPTION_RETRY_SERVICE_H_

#include <cstdint>

#include "esp_err.h"

namespace transcription_retry_service {

// Snapshot of the last (or currently running) reconnect-triggered retry batch. A "batch" is one
// pass over every archived recording flagged pending_transcription, run once per Gemini
// ready-edge (see main/app_shell.cpp's HandleGeminiEvent).
struct Snapshot {
    bool initialized = false;
    bool batch_in_flight = false;
    int last_batch_attempted = 0;
    int last_batch_succeeded = 0;
    int last_batch_failed = 0;
    // Bumped once per completed batch so subscribers can edge-detect "a batch just finished"
    // instead of re-firing on every unrelated snapshot read.
    uint32_t last_batch_generation = 0;
};

struct Event {
    Snapshot snapshot = {};
};

using EventHandler = void (*)(const Event& event, void* context);

esp_err_t Init();
void SetEventHandler(EventHandler handler, void* context);
Snapshot GetSnapshot();

// Kicks a background retry pass over every archived recording with pending_transcription=true.
// No-op (returns false immediately) if a batch is already running or nothing is pending.
bool RetryPending();

}  // namespace transcription_retry_service

#endif  // TRANSCRIPTION_RETRY_SERVICE_H_
