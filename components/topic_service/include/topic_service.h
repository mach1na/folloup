#ifndef TOPIC_SERVICE_H_
#define TOPIC_SERVICE_H_

#include <cstdint>
#include <string>
#include <vector>

#include "esp_err.h"

namespace topic_service {

// A user-defined categorization label, orthogonal to RecordingMetadata::tag (Note/Idea/Task).
// Entries reference a topic by `id`, never by `name` -- see topic_service.cpp for why (renaming
// or deleting a topic must never require rewriting every recording sidecar that references it).
struct Topic {
    std::string id = {};
    std::string name = {};
    int64_t created_unix_seconds = 0;
};

struct Snapshot {
    bool initialized = false;
    bool storage_available = false;
    std::vector<Topic> topics = {};
};

struct Event {
    Snapshot snapshot = {};
};

using EventHandler = void (*)(const Event& event, void* context);

// Loads the topic registry from SD (seeding three default topics -- Home, Car, Shopping -- the
// first time it runs and finds no registry file). Runs SD I/O on the caller's task; safe to call
// once at startup, after storage_service::Init().
esp_err_t Init();
void SetEventHandler(EventHandler handler, void* context);
Snapshot GetSnapshot();

// Creates a new topic and persists the registry immediately. Returns the new topic's id, or an
// empty string on failure (storage unavailable, empty name). Runs SD I/O on the caller's task;
// call from a non-UI task.
std::string Create(const std::string& name);
// Renames an existing topic in place -- referencing recordings need no changes, since they store
// the topic id, not its name. Returns false if the id isn't found or storage is unavailable.
bool Rename(const std::string& id, const std::string& name);
// Removes a topic from the registry. Recordings that still reference this id keep the now-
// orphaned reference; readers resolve ids against GetSnapshot().topics and simply skip one that
// isn't there, so no sidecar rewrite is needed. Returns false if the id isn't found.
bool Delete(const std::string& id);

}  // namespace topic_service

#endif  // TOPIC_SERVICE_H_
