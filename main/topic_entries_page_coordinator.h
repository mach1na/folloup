#ifndef TOPIC_ENTRIES_PAGE_COORDINATOR_H_
#define TOPIC_ENTRIES_PAGE_COORDINATOR_H_

#include <string>
#include <vector>

#include "epaper_ui/topic_entries_page.h"
#include "page_navigation/navigation_model.h"
#include "page_navigation/roving_focus.h"
#include "recording_archive_service.h"
#include "timeline_group_focus.h"

// Owns one topic's filtered, day-grouped timeline: every recording (Note/Idea/Task alike) whose
// RecordingMetadata::topic_ids contains the current topic id. The two-level focus (date-chip
// groups -> item rows) reuses the same TimelineGroupFocus Notes/Todos/Follow-up already share;
// this class owns only what's topic-specific: the topic_id filter, per-entry rendering, and the
// page-owned Back button (this page has exactly one source -- the Topics browse screen -- so no
// DetailsPageSource-style enum is needed; Back composes as an extra top-level nav item alongside
// the date groups, same pattern Todos' segment control uses).
class TopicEntriesPageCoordinator {
public:
    struct TimelineEntry {
        epaper_ui::ListItemState item = {};
        std::string recording_id = {};
    };
    using TimelineGroup = TimelineGroupFocus<TimelineEntry>::Group;

    TopicEntriesPageCoordinator();

    // Stash the target topic before the page is shown (the archive read happens in Show()).
    void QueueShow(const std::string& topic_id, const std::string& topic_name);
    void Show(const std::vector<recording_archive_service::RecordingEntry>& recordings);
    void RefreshFromArchive(
        const std::vector<recording_archive_service::RecordingEntry>& recordings);

    bool MoveFocus(int delta) { return focus_.MoveFocus(delta); }
    bool SetFocusIndex(int index) { return focus_.SetFocusIndex(index); }
    bool IsRoleFocused(page_navigation::NavigationItemRole role) const
    {
        return focus_.IsRoleFocused(role);
    }
    bool EnterFocusedGroup() { return focus_.EnterFocusedGroup(); }
    bool ExitItemList() { return focus_.ExitItemList(); }

    epaper_ui::TopicEntriesPageState BuildState() const;

    const TimelineEntry* SelectedEntry() const { return focus_.SelectedEntry(); }
    bool item_list_active() const { return focus_.ItemListActive(); }
    const std::string& topic_id() const { return topic_id_; }

    const page_navigation::NavigationModel& navigation_model() const
    {
        return focus_.navigation_model();
    }
    const page_navigation::RovingFocus& focus() const { return focus_.focus(); }

private:
    std::vector<TimelineGroup> BuildGroups(
        const std::vector<recording_archive_service::RecordingEntry>& recordings) const;

    TimelineGroupFocus<TimelineEntry> focus_;
    std::string pending_topic_id_ = {};
    std::string pending_topic_name_ = {};
    std::string topic_id_ = {};
    std::string topic_name_ = "Topic";
};

#endif  // TOPIC_ENTRIES_PAGE_COORDINATOR_H_
